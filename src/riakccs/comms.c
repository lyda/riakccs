#include <assert.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#ifndef S_SPLINT_S
#include <unistd.h>
#else
extern char *strdup(char *s);
#endif /* S_SPLINT_S */

#include "riak_dt.pb-c.h"
#include "riak_kv.pb-c.h"
#include "riak.pb-c.h"
#include "riak_search.pb-c.h"
#include "riak_yokozuna.pb-c.h"
#include "riakccs/api.h"
#include "riakccs/pb.h"
#include "riakccs/debug.h"

#define SDEF(T) [(T)] = (#T)
static const char *MC_str[] = {
  SDEF(MC_RpbErrorResp),
  SDEF(MC_RpbPingReq),
  SDEF(MC_RpbPingResp),
  SDEF(MC_RpbGetClientIdReq),
  SDEF(MC_RpbGetClientIdResp),
  SDEF(MC_RpbSetClientIdReq),
  SDEF(MC_RpbSetClientIdResp),
  SDEF(MC_RpbGetServerInfoReq),
  SDEF(MC_RpbGetServerInfoResp),
  SDEF(MC_RpbGetReq),
  SDEF(MC_RpbGetResp),
  SDEF(MC_RpbPutReq),
  SDEF(MC_RpbPutResp),
  SDEF(MC_RpbDelReq),
  SDEF(MC_RpbDelResp),
  SDEF(MC_RpbListBucketsReq),
  SDEF(MC_RpbListBucketsResp),
  SDEF(MC_RpbListKeysReq),
  SDEF(MC_RpbListKeysResp),
  SDEF(MC_RpbGetBucketReq),
  SDEF(MC_RpbGetBucketResp),
  SDEF(MC_RpbSetBucketReq),
  SDEF(MC_RpbSetBucketResp),
  SDEF(MC_RpbMapRedReq),
  SDEF(MC_RpbMapRedResp),
  SDEF(MC_RpbIndexReq),
  SDEF(MC_RpbIndexResp),
  SDEF(MC_RpbSearchQueryReq),
  SDEF(MC_RpbSearchQueryResp),
  SDEF(MC_RpbLibError),
  [MC_RpbMAX] = NULL
};
#undef SDEF

/** \brief Translate riak message codes to strings.
 *
 * \param mc Message code.
 */
const char *
riak_mc2str(uint8_t mc)
{
  static char *lib_bad_mc_error = "MC_RpbLibBadMCError";

  if (mc < 0 || mc >= MC_RpbMAX) {
    return lib_bad_mc_error;
  }
  return MC_str[mc];
}

/** \brief Return a string for an unknown message code.
 *
 * \param rc RiakClient object - for the allocator.
 */
void
unknown_mc(RiakResponse *rv)
{
  char *msgfmt = "Unknown or unexpected mc (%s)";
  char *msg;
  const char *mc_str = riak_mc2str(rv->mc);

  msg = rv->_rs->_rc->allocator->alloc(
      rv->_rs->_rc->allocator->allocator_data,
      strlen(msgfmt) + strlen(mc_str) + 1);
  snprintf(msg, strlen(msgfmt) + strlen(mc_str), msgfmt, mc_str);
  rv->liberr.msg = msg;
  rv->liberr.mc = rv->mc;
  rv->mc = MC_RpbLibError;
}

/** \brief Connect to a tcp/ip port.
 *
 * \param host Host to connect to.
 * \param port Port to connect to.
 */
static int
connect_to_host(char *host, char *port)
{
  int sd = -1, lookup_err;
  struct addrinfo hints;
  struct addrinfo *result, *rp;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;

  lookup_err = getaddrinfo(host, port, &hints, &result);
  if (lookup_err != 0) {
    // TODO: Log connection failure.
    fprintf(stderr, "Failed to look up %s:%s (%s)",
        host, port, gai_strerror(lookup_err));
    return -1;
  }

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      // TODO: Log connection failure.
      fprintf(stderr, "Couldn't create socket");
      return -1;
    } else {
      if (connect(sd, rp->ai_addr, rp->ai_addrlen) != -1) {
        break;
      } else {
        close(sd);
        sd = -1;
      }
    }
  }

  if (rp == NULL) {
    // TODO: Log connection failure.
  }

  freeaddrinfo(result);
  return sd;
}

/** \brief Write a request to the Riak server.
 *
 * Updates \c bytes_tot with the total number of bytes written (including
 * header) and returns RiakSession on success or NULL on failure.

 * \param rc RiakClient object.
 * \param mc The message code to send.
 * \param len Length of the data to send.
 * \param pb The serialised protobuf data.
 * \param bytes_tot Number of bytes written.
 */
static RiakSession *
_write_req(RiakClient *rc,
    RiakResponse *rv,
    uint8_t mc,
    uint32_t len,
    uint8_t *pb)
{
  RiakSession *rs;
  int server;
  uint8_t hdr[5];
  uint32_t *hdr_len;
  ssize_t bytes = 0;
  size_t bytes_tot;

  if (!rv) {
    rs = malloc(sizeof(RiakSession));
    rs->_rc = rc;
    if (mc != MC_RpbIndexReq) {
      rs->streaming = 0;
    } else {
      rs->streaming = 1;
    }

    /* Try to find an available server. */
    assert(rc->current < rc->n_servers);
    server = rc->current;
    do {
      server++;
      if (server >= rc->n_servers) {
        server = 0;
      }
    } while (server != rc->current
        && rc->servers[server].inuse
        && rc->servers[server].sd < 0);

    /* If server is available, assign it to session and mark it inuse.
     * Otherwise make a new connection. */
    if (rc->servers[server].host
        && rc->servers[server].sd >= 0
        && !rc->servers[server].inuse) {
      rc->servers[server].inuse = 1;
      rs->server = server;
      rs->sd = rc->servers[server].sd;
    } else {
      rs->server = -1;
      server = rc->current;
      /* Find first valid host in server list after rc->current. */
      do {
        server++;
        if (server >= rc->n_servers) {
          server = 0;
        }
      } while (server != rc->current
          && !rc->servers[server].host);
      if (rc->servers[server].host) {
        rs->sd = connect_to_host(rc->servers[server].host,
            rc->servers[server].port);
      } else {
        /* TODO: Log error - no available servers. */
        free(rs);
        return NULL;
      }
    }

    /* Increment rc->current to next server */
    rc->current++;
    if (rc->current == rc->n_servers) {
      rc->current = 0;
    }
  } else {
    rs = rv->_rs;
    riak_response_only_free(rc, rv);
  }


  /* Create header block. */
  hdr_len = (uint32_t *)hdr;
  *hdr_len = htonl(len + 1);
  hdr[4] = mc;

  /* Write header. */
  bytes_tot = 0;
  while (bytes_tot < 5) {
    bytes = write(rs->sd, hdr + bytes_tot, 5 - bytes_tot);
    if (bytes <= 0) {
      // TODO: Log error; error out server.
      free(rs);
      return NULL;
    }
    bytes_tot += bytes;
  }

  /* Write body (if it exists). */
  bytes_tot = 0;
  if (len) {
    while (bytes_tot < len) {
      bytes = write(rs->sd, pb + bytes_tot, len - bytes_tot);
      if (bytes <= 0) {
        // TODO: Log error; error out server.
        free(rs);
        return NULL;
      }
      bytes_tot += bytes;
    }
  }

  return rs;
}

/** \brief Read a response from the Riak server.
 *
 * Returns a RiakResponse object on success or NULL on failure.
 *
 * \param rs RiakSession object.
 */
static RiakResponse *
_read_resp(RiakSession *rs)
{
  RiakResponse *rv;
  uint8_t hdr[5], *pb;
  ssize_t bytes = 0;
  size_t bytes_tot = 0, len;
  uint32_t *hdr_len;
  int release_socket = 1;  /* Default to release. */

  rv = rs->_rc->allocator->alloc(rs->_rc->allocator->allocator_data,
      sizeof(RiakResponse));
  rv->success = 1;
  rv->_rs = rs;

  /* Read header. */
  while (bytes_tot < 5) {
    bytes = read(rs->sd, hdr + bytes_tot, 5 - bytes_tot);
    if (bytes <= 0) {
      // TODO: Log out error; error out server.
      rs->_rc->allocator->free(rs->_rc->allocator->allocator_data, rv);
      rs->_rc->last_erract = RIAK_ACT_READ_HDR;
      rs->_rc->last_errbytes = bytes;
      return NULL;
    }
    bytes_tot += bytes;
  }

  /* Process header. */
  hdr_len = (uint32_t *)hdr;
  len = ntohl(*hdr_len);
  if (len == 0) {
    rs->_rc->last_erract = RIAK_ACT_READ_PROC_HDR;
    rs->_rc->last_errbytes = bytes;
    rs->_rc->allocator->free(rs->_rc->allocator->allocator_data, rv);
    return NULL;
  }
  len -= 1;
  rv->mc = hdr[4];

  /* Read body (if it exists). */
  pb = NULL;  /* pb is NULL unless something is allocated. */
  if (len) {
    pb = rs->_rc->allocator->alloc(rs->_rc->allocator->allocator_data, (len));
    bytes_tot = 0;
    while (bytes_tot < len) {
      bytes = read(rs->sd, pb + bytes_tot, len - bytes_tot);
      if (bytes <= 0) {
        rs->_rc->allocator->free(rs->_rc->allocator->allocator_data, pb);
        pb = NULL;
        rs->_rc->allocator->free(rs->_rc->allocator->allocator_data, rv);
        rs->_rc->last_erract = RIAK_ACT_READ_PB;
        rs->_rc->last_errbytes = bytes;
        return NULL;
      }
      bytes_tot += bytes;
    }
  }

  switch (rv->mc) {
    case MC_RpbPingResp:
    case MC_RpbSetBucketResp:
    case MC_RpbDelResp:
    case MC_RpbSetClientIdResp:
      break;
    case MC_RpbErrorResp:
      rv->err.resp = rpb_error_resp__unpack(rs->_rc->allocator, len, pb);
      break;
    case MC_RpbListBucketsResp:
      rv->bl.resp = rpb_list_buckets_resp__unpack(rs->_rc->allocator,
          len, pb);
      break;
    case MC_RpbListKeysResp:
      rv->kl.resp = rpb_list_keys_resp__unpack(rs->_rc->allocator,
          len, pb);
      if (!rv->kl.resp->has_done
          || (rv->kl.resp->has_done && !rv->kl.resp->done)) {
        release_socket = 0;
      }
      break;
    case MC_RpbGetBucketResp:
      rv->bp.resp = rpb_get_bucket_resp__unpack(rs->_rc->allocator,
          len, pb);
      break;
    case MC_RpbGetResp:
      rv->g.resp = rpb_get_resp__unpack(rs->_rc->allocator, len, pb);
      break;
    case MC_RpbPutResp:
      rv->p.resp = rpb_put_resp__unpack(rs->_rc->allocator, len, pb);
      break;
    case MC_RpbMapRedResp:
      rv->mr.resp = rpb_map_red_resp__unpack(rs->_rc->allocator, len, pb);
      if (!rv->mr.resp->has_done
          || (rv->mr.resp->has_done && !rv->mr.resp->done)) {
        release_socket = 0;
      }
      break;
    case MC_RpbIndexResp:
      rv->i.resp = rpb_index_resp__unpack(rs->_rc->allocator, len, pb);
      if (rs->streaming
          && (!rv->i.resp->has_done
            || (rv->i.resp->has_done && !rv->i.resp->done))) {
        release_socket = 0;
      }
      break;
    case MC_RpbSearchQueryResp:
      rv->s.resp = rpb_search_query_resp__unpack(rs->_rc->allocator,
          len, pb);
      break;
    case MC_RpbGetClientIdResp:
      rv->gc.resp = rpb_get_client_id_resp__unpack(rs->_rc->allocator,
          len, pb);
      break;
    case MC_RpbGetServerInfoResp:
      rv->si.resp = rpb_get_server_info_resp__unpack(rs->_rc->allocator,
          len, pb);
      break;
    default:
      rv->success = 0;
      unknown_mc(rv);
      break;
  }

  if (release_socket) {
    if (rs->server < 0) {
      /* Dynamically allocated server. */
      close(rs->sd);
    } else {
      rs->_rc->servers[rs->server].inuse = 0;
    }
  }

  if (pb) {
    rs->_rc->allocator->free(rs->_rc->allocator->allocator_data, pb);
  }
  return rv;
}


/** \brief Create RiakClient object.
 *
 * \param allocator An allocator. If set to NULL, uses the default
 *                  allocator.
 * \param max_servers Maximum number of Riak servers to add.
 *
 * Things to think about:
 *   \li Reconnections. Manual or automatic? Perhaps configuarable.
 *   \li Rotating servers. Strategy? Settable per function?
 */
RiakClient *
riak_client_init(ProtobufCAllocator *allocator, int max_servers)
{
  RiakClient *rc;
  int i;

  assert(max_servers > 0);

  /* Allocate RiakClient object and assign allocator. */
  if (allocator == NULL) {
    allocator = &protobuf_c_default_allocator;
  }
  rc = allocator->alloc(allocator->allocator_data, sizeof(RiakClient));
  if (!rc) {
    return NULL;
  }
  rc->allocator = allocator;

  /* Allocate servers. */
  rc->servers = rc->allocator->alloc(allocator->allocator_data,
      sizeof(struct _RiakServer) * max_servers);
  if (!rc->servers) {
    allocator->free(allocator->allocator_data, rc);
    return NULL;
  }
  memset(rc->servers, 0, sizeof(struct _RiakServer) * max_servers);
  rc->n_servers = max_servers;
  for (i = 0; i < rc->n_servers; i++) {
    rc->servers[i].sd = -1;
  }

  /* Assign functions. */
  rc->_write = &_write_req;
  rc->_read = &_read_resp;

  /* Assign initial accounting data. */
  rc->current = -1;
  rc->last_errno = 0;
  rc->last_erract = 0;
  rc->last_errbytes = 0;

  return rc;
}


/** \brief Add and connect to a server given by host/port.
 *
 * Returns number of servers added.  It would be 1 for success, 0 for
 * failure.
 *
 * \param rc Riak client structure. This is also freed.
 * \param host Host to connect to.
 * \param port Port to connect to.
 */
int
riak_server_add(RiakClient *rc, char *host, char *port)
{
  int i;

  for (i = 0; i < rc->n_servers; i++) {
    if (!rc->servers[i].host) {
      rc->servers[i].host = strdup(host);
      rc->servers[i].port = strdup(port);
      if (!rc->servers[i].host || !rc->servers[i].port) {
        free(rc->servers[i].host);
        free(rc->servers[i].port);
        rc->servers[i].host = NULL;
        rc->servers[i].port = NULL;
      } else {
        rc->servers[i].sd = connect_to_host(host, port);
        rc->servers[i].inuse = 0;
        rc->current = i;
        return 1;
      }
    }
  }

  return 0;
}


/** \brief Removes server that matches a given host/port.
 *
 * Returns 1 if server disconnected, 0 if none.  Call multiple
 * times to remove all servers that match this host/port.
 *
 * \param rc Riak client structure. This is also freed.
 * \param host Host to match.
 * \param port Port to match.
 */
int
riak_server_del(RiakClient *rc, char *host, char *port)
{
  int i, j;

  for (i = 0; i < rc->n_servers; i++) {
    if (rc->servers[i].host) {
      if ((strcmp(host, rc->servers[i].host) == 0)
          && (strcmp(port, rc->servers[i].port) == 0)) {
        /* This is the server to be deleted. */
        free(rc->servers[i].host);
        free(rc->servers[i].port);
        rc->servers[i].host = NULL;
        rc->servers[i].port = NULL;
        rc->servers[i].sd = -1;
        rc->servers[i].inuse = 0;
        return 1;
      }
    }
  }

  return 0;
}

/** \brief Returns the number of currently known servers.
 *
 * \param rc Riak client structure.
 */
int
riak_servers_known(RiakClient *rc)
{
  int i, server_ct = 0;

  for (i = 0; i < rc->n_servers; i++) {
    if (rc->servers[i].host) {
      server_ct++;
    }
  }
  return server_ct;
}

/** \brief Returns the number of currently connected servers.
 *
 * \param rc Riak client structure.
 */
int
riak_servers_active(RiakClient *rc)
{
  int i, server_ct = 0;

  for (i = 0; i < rc->n_servers; i++) {
    if (rc->servers[i].sd >= 0) {
      server_ct++;
    }
  }
  return server_ct;
}

/** \brief Disconnects from all connected servers and frees allocated data.
 *
 * \param rc Riak client structure. This is also freed.
 */
void
riak_servers_disconnect(RiakClient *rc)
{
  int i;

  for (i = 0; i < rc->n_servers; i++) {
    if (rc->servers[i].host) {
      /* This is not a deleted server - delete it. */
      rc->allocator->free(rc->allocator->allocator_data, rc->servers[i].host);
      rc->allocator->free(rc->allocator->allocator_data, rc->servers[i].port);
      if (rc->servers[i].sd >= 0) {
        close(rc->servers[i].sd);
      }
    }
  }
  rc->allocator->free(rc->allocator->allocator_data, rc);
}

/** \brief Frees the memory allocated for the RiakResponse.
 *
 * This will not free the associated RiakSession object.
 *
 * Note that it needs the RiakClient object so that it can call the
 * correct deallocator.
 *
 * \param rc RiakClient object - for the allocator.
 * \param rv The data to free.
 */
void
riak_response_only_free(RiakClient *rc, RiakResponse *rv)
{
  rc->last_errno = 0;
  switch (rv->mc) {
    case MC_RpbListBucketsResp:
      rpb_list_buckets_resp__free_unpacked(rv->bl.resp, rc->allocator);
      break;
    case MC_RpbListKeysResp:
      rpb_list_keys_resp__free_unpacked(rv->kl.resp, rc->allocator);
      break;
    case MC_RpbGetClientIdResp:
      rpb_get_client_id_resp__free_unpacked(rv->gc.resp, rc->allocator);
      break;
    case MC_RpbGetServerInfoResp:
      rpb_get_server_info_resp__free_unpacked(rv->si.resp, rc->allocator);
      break;
    case MC_RpbGetResp:
      rpb_get_resp__free_unpacked(rv->g.resp, rc->allocator);
      break;
    case MC_RpbPutResp:
      rpb_put_resp__free_unpacked(rv->p.resp, rc->allocator);
      break;
    case MC_RpbErrorResp:
      rpb_error_resp__free_unpacked(rv->err.resp, rc->allocator);
      break;
    case MC_RpbGetBucketResp:
      rpb_get_bucket_resp__free_unpacked(rv->bp.resp, rc->allocator);
      break;
    case MC_RpbMapRedResp:
      rpb_map_red_resp__free_unpacked(rv->mr.resp, rc->allocator);
      break;
    case MC_RpbIndexResp:
      rpb_index_resp__free_unpacked(rv->i.resp, rc->allocator);
      break;
    case MC_RpbSearchQueryResp:
      rpb_search_query_resp__free_unpacked(rv->s.resp, rc->allocator);
      break;
    case MC_RpbLibError:
      rc->allocator->free(rc->allocator->allocator_data, rv->liberr.msg);
      break;
    case MC_RpbPingResp:
    case MC_RpbDelResp:
    case MC_RpbSetBucketResp:
    case MC_RpbSetClientIdResp:
      /* Nothing to free here. */
      break;
    default:
      rc->last_errno = -1;
      rc->last_erract = RIAK_ACT_FREE;
      break;
  }
  rc->allocator->free(rc->allocator->allocator_data, rv);
}

/** \brief Frees the memory allocated for the RiakResponse.
 *
 * Note that it needs the RiakClient object so that it can call the
 * correct deallocator.
 *
 * \param rc RiakClient object - for the allocator.
 * \param rv The data to free.
 */
void
riak_response_free(RiakClient *rc, RiakResponse *rv)
{
  // TODO: Make sure session is finished here.
  rc->allocator->free(rc->allocator->allocator_data, rv->_rs);
  riak_response_only_free(rc, rv);
}
