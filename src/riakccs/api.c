/** \file
 *
 * \brief Module for communicating with riak with protocol buffers.
 *
 * Functions to connect to riak servers and send and receive
 * all supported protocol buffers.
 *
 * See the riak docs for more info on the protocol:
 *   http://docs.basho.com/riak/latest/dev/references/protocol-buffers/
 *
 * The summary though is that it works like so (and because of this
 * I cannot use protoc-c's builtin RPC services):
 *
 *   \li 4 bytes: message code + pb length in network byte order
 *   \li 1 byte: message code (see MC_* defines below)
 *   \li n bytes: protocol buffer
 */

/** \mainpage
 *
 * Riak C library.
 */

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

/** \brief Send a ping request.
 *
 * Check rv->mc to see if the function succeeded (MC_RpbPingResp) or
 * failed (MC_RpbErrorResp).  Make sure rv->success is 1 to confirm
 * the ping.
 *
 * \param rc Riak client object.
 */
RiakResponse *
riak_ping(RiakClient *rc)
{
  RiakSession *rs;
  RiakResponse *rv;

  /* Send a ping request. */
  rs = rc->_write(rc, NULL, MC_RpbPingReq, 0, NULL);
  if (!rs) {
    // TODO: log error.
    return NULL;
  }

  /* Retrieve a get client id response. */
  rv = rc->_read(rs);
  if (!rv) {
    // TODO: log error.
    return NULL;
  }

  return rv;
}

/** \brief Retrieve a list of buckets.
 *
 * Check rv->mc to see if the function succeeded (MC_RpbListBucketsResp)
 * or failed (MC_RpbErrorResp).  It will have an RpbListBucketsResp
 * in rv->bl.resp.
 *
 * \param rc Riak client object.
 *
 * See proto/riak_kv.proto for the fields.
 */
RiakResponse *
riak_list_buckets(RiakClient *rc)
{
  RiakSession *rs;
  RiakResponse *rv;

  /* Make and send a list buckets request. */
  rs = rc->_write(rc, NULL, MC_RpbListBucketsReq, 0, NULL);
  if (!rs) {
    // TODO: log error.
    return NULL;
  }

  /* Retrieve a list buckets response. */
  rv = rc->_read(rs);
  if (!rv) {
    // TODO: log error.
    return NULL;
  }

  return rv;
}

/** \brief STREAMING: Retrieve a list of keys in a bucket.
 *
 * Check rv->mc to see if the function succeeded (MC_RpbListKeysResp)
 * or failed (MC_RpbErrorResp).  It will have an RpbListKeysResp in
 * rv->kl.resp.
 *
 * \param rc Riak client object.
 * \param rv Call with NULL initially. When getting further
 *           responses, call with the previous return value of this
 *           function.
 * \param bucket Bucket to list keys from.
 *
 * See proto/riak_kv.proto for the fields.
 */
RiakResponse *
riak_list_keys(RiakClient *rc, RiakResponse *rv, unsigned char *bucket)
{
  uint8_t *pb;
  size_t len;
  RiakSession *rs;

  if (!rv) {
    RpbListKeysReq req = RPB_LIST_KEYS_REQ__INIT;

    /* Make and send a list keys request. */
    str2pbbd(&req.bucket, bucket);
    len = rpb_list_keys_req__get_packed_size(&req);
    pb = rc->allocator->alloc(rc->allocator->allocator_data, len);
    (void)rpb_list_keys_req__pack(&req, pb);
    rs = rc->_write(rc, NULL, MC_RpbListKeysReq, len, pb);
    rc->allocator->free(rc->allocator->allocator_data, pb);
    if (!rs) {
      // TODO: log error.
      return NULL;
    }
  } else {
    rs = rv->_rs;
    riak_response_only_free(rc, rv);
  }

  /* Retrieve a list keys response. */
  rv = rc->_read(rs);
  if (!rv) {
    // TODO: log error.
    return NULL;
  }

  return rv;
}

/** \brief Retrieve the properties of a bucket.
 *
 * Check rv->mc to see if the function succeeded (MC_RpbGetBucketResp)
 * or failed (MC_RpbErrorResp).  It will have an RpbGetBucketResp in
 * rv->bp.resp.
 *
 * \param rc Riak client object.
 * \param bucket Bucket to get property for.
 *
 * See proto/riak_kv.proto for the fields.
 */
RiakResponse *
riak_get_bucket_props(RiakClient *rc, unsigned char *bucket)
{
  uint8_t *pb;
  size_t len;
  RpbGetBucketReq req = RPB_GET_BUCKET_REQ__INIT;
  RiakSession *rs;
  RiakResponse *rv;

  /* Make and send a get bucket props request. */
  str2pbbd(&req.bucket, bucket);
  len = rpb_get_bucket_req__get_packed_size(&req);
  pb = rc->allocator->alloc(rc->allocator->allocator_data, len);
  (void)rpb_get_bucket_req__pack(&req, pb);
  rs = rc->_write(rc, NULL, MC_RpbGetBucketReq, len, pb);
  rc->allocator->free(rc->allocator->allocator_data, pb);
  if (!rs) {
    // TODO: log error.
    return NULL;
  }

  /* Retrieve a get bucket props response. */
  rv = rc->_read(rs);
  if (!rv) {
    // TODO: log error.
    return NULL;
  }

  return rv;
}

/** \brief Set the properties of a bucket.
 *
 * Check rv->mc to see if the function succeeded (MC_RpbSetBucketResp)
 * or failed (MC_RpbErrorResp).  It will have rv->success set to 1 on
 * success.
 *
 * \param rc Riak client object.
 * \param bucket Bucket to get property for.
 * \param props RpbBucketProps protobuf.
 *
 * See proto/riak_kv.proto for the fields.
 */
RiakResponse *
riak_set_bucket_props(RiakClient *rc,
    unsigned char *bucket,
    RpbBucketProps *props)
{
  uint8_t *pb;
  size_t len;
  RpbSetBucketReq req = RPB_SET_BUCKET_REQ__INIT;
  RiakSession *rs;
  RiakResponse *rv;

  /* Make and send a set bucket props request. */
  str2pbbd(&req.bucket, bucket);
  req.props = props;
  len = rpb_set_bucket_req__get_packed_size(&req);
  pb = rc->allocator->alloc(rc->allocator->allocator_data, len);
  (void)rpb_set_bucket_req__pack(&req, pb);
  rs = rc->_write(rc, NULL, MC_RpbSetBucketReq, len, pb);
  rc->allocator->free(rc->allocator->allocator_data, pb);
  if (!rs) {
    // TODO: log error.
    return NULL;
  }

  /* Retrieve a set bucket props response. */
  rv = rc->_read(rs);
  if (!rv) {
    // TODO: log error.
    return NULL;
  }

  return rv;
}

/** \brief Retrieve object from a bucket/key.
 *
 * Check rv->mc to see if the function succeeded (MC_RpbGetResp)
 * or failed (MC_RpbErrorResp).  It will have an RpbGetResp in
 * rv->g.resp.
 *
 * \param rc Riak client object.
 * \param bucket Bucket to look for key in.
 * \param key Key to fetch.
 * \param req A RpbGetReq protobuf.
 *
 * See proto/riak_kv.proto for the fields.
 */
RiakResponse *
riak_fetch_object_full(RiakClient *rc,
    unsigned char *bucket,
    ProtobufCBinaryData *key,
    RpbGetReq *req)
{
  uint8_t *pb;
  size_t len;
  RiakSession *rs;
  RiakResponse *rv;

  /* Make and send a get request. */
  str2pbbd(&req->bucket, bucket);
  req->key.data = key->data;
  req->key.len = key->len;
  len = rpb_get_req__get_packed_size(req);
  pb = rc->allocator->alloc(rc->allocator->allocator_data, len);
  (void)rpb_get_req__pack(req, pb);
  rs = rc->_write(rc, NULL, MC_RpbGetReq, len, pb);
  rc->allocator->free(rc->allocator->allocator_data, pb);
  if (!rs) {
    // TODO: log error.
    return NULL;
  }

  /* Retrieve the get response. */
  rv = rc->_read(rs);
  if (!rv) {
    // TODO: log error.
    return NULL;
  }

  return rv;
}

/** \brief Store an object in riak.
 *
 * Check rv->mc to see if the function succeeded (MC_RpbPutResp) or
 * failed (MC_RpbErrorResp).
 *
 * \param rc Riak client object.
 * \param req Protocol buffer with the object to add.
 */
RiakResponse *
riak_store_object_full(RiakClient *rc, RpbPutReq *req)
{
  uint8_t *pb, mc;
  int sd, err;
  size_t len, bytes_tot;
  RiakSession *rs;
  RiakResponse *rv;

  /* Send a put request. */
  len = rpb_put_req__get_packed_size(req);
  pb = rc->allocator->alloc(rc->allocator->allocator_data, len);
  (void)rpb_put_req__pack(req, pb);
  rs = rc->_write(rc, NULL, MC_RpbPutReq, len, pb);
  rc->allocator->free(rc->allocator->allocator_data, pb);
  if (!rs) {
    // TODO: log error.
    return NULL;
  }

  /* Retrieve the put response. */
  rv = rc->_read(rs);
  if (!rv) {
    // TODO: log error.
    return NULL;
  }

  return rv;
}

/** \brief Delete object based on a bucket and a key.
 *
 * \param rc Riak client object.
 * \param req A RpbDelReq detailing what bucket/key to be deleted.
 */
RiakResponse *
riak_delete_object_full(RiakClient *rc,
    RpbDelReq *req)
{
  uint8_t *pb;
  size_t len;
  RiakSession *rs;
  RiakResponse *rv;

  /* Make and send a del request. */
  len = rpb_del_req__get_packed_size(req);
  pb = rc->allocator->alloc(rc->allocator->allocator_data, len);
  (void)rpb_del_req__pack(req, pb);
  rs = rc->_write(rc, NULL, MC_RpbDelReq, len, pb);
  rc->allocator->free(rc->allocator->allocator_data, pb);
  if (!rs) {
    // TODO: log error.
    return NULL;
  }

  /* Retrieve the del response code. */
  rv = rc->_read(rs);
  if (!rv) {
    // TODO: log error.
    return NULL;
  }

  return rv;
}

/** \brief STREAMING: Send a map reduce request.
 *
 * The return value will either be an \c MC_RpbMapRedResp or an
 * \c MC_RpbErrorResp.
 *
 * \param rc Riak client.
 * \param rv Call with NULL initially. When getting further
 *           responses, call with the previous return value of this
 *           function.
 * \param request The json or erlang code to use for the map
 *                reduce job.
 * \param content_type \c application/json or \c
 *        application/x-erlang-binary.
 */
RiakResponse *
riak_map_reduce(RiakClient *rc, RiakResponse *rv,
    unsigned char *request, unsigned char *content_type)
{
  uint8_t *pb;
  size_t len;
  RiakSession *rs;

  if (!rv) {
    RpbMapRedReq req = RPB_MAP_RED_REQ__INIT;

    /* Make and send a map reduce request. */
    str2pbbd(&req.request, request);
    /* TODO: Check this for validity.  Or maybe this should be an enum? */
    str2pbbd(&req.content_type, content_type);
    len = rpb_map_red_req__get_packed_size(&req);
    pb = rc->allocator->alloc(rc->allocator->allocator_data, len);
    (void)rpb_map_red_req__pack(&req, pb);
    rs = rc->_write(rc, NULL, MC_RpbMapRedReq, len, pb);
    rc->allocator->free(rc->allocator->allocator_data, pb);
    if (!rs) {
      // TODO: log error.
      return NULL;
    }
  } else {
    /* Reserve a riak socket. */
    rs = rv->_rs;
    riak_response_only_free(rc, rv);
  }

  /* Retrieve a map reduce response. */
  rv = rc->_read(rs);
  if (!rv) {
    // TODO: log error.
    return NULL;
  }

  return rv;
}

/** \brief STREAMING: Retrieve secondary indexes.
 *
 * Check rv->mc to see if the function succeeded (MC_RpbIndexResp) or
 * failed (MC_RpbErrorResp).
 *
 * \param rc Riak client object.
 * \param rv Call with NULL initially. When getting further
 *           responses, call with the previous return value of this
 *           function.
 * \param req Protocol buffer with the secondary index query.
 *
 * See proto/riak_kv.proto for the fields.
 */
RiakResponse *
riak_secondary_indexes(RiakClient *rc, RiakResponse *rv, RpbIndexReq *req)
{
  uint8_t *pb;
  size_t len;
  RiakSession *rs;

  /* Send secondary index request. */
  len = rpb_index_req__get_packed_size(req);
  pb = rc->allocator->alloc(rc->allocator->allocator_data, len);
  (void)rpb_index_req__pack(req, pb);
  if (rv) {
    rs = rc->_write(rc, rv, MC_RpbIndexReq, len, pb);
  } else {
    rs = rc->_write(rc, NULL, MC_RpbIndexReq, len, pb);
  }
  rc->allocator->free(rc->allocator->allocator_data, pb);
  if (!rs) {
    // TODO: log error.
    return NULL;
  }

  /* Retrieve secondary index response. */
  rv = rc->_read(rs);
  if (!rv) {
    // TODO: log error.
    return NULL;
  }

  return rv;
}

/** \brief Search Riak.
 *
 * Check rv->mc to see if the function succeeded
 * ( \c MC_RpbSearchQueryResp) or failed ( \c MC_RpbErrorResp).
 *
 * \param rc Riak client object.
 * \param req Protobuf with the search query.
 */
RiakResponse *
riak_search(RiakClient *rc, RpbSearchQueryReq *req)
{
  uint8_t *pb;
  size_t len;
  RiakSession *rs;
  RiakResponse *rv;

  /* Send a search request. */
  len = rpb_search_query_req__get_packed_size(req);
  pb = rc->allocator->alloc(rc->allocator->allocator_data, len);
  (void)rpb_search_query_req__pack(req, pb);
  rs = rc->_write(rc, NULL, MC_RpbSearchQueryReq, len, pb);
  rc->allocator->free(rc->allocator->allocator_data, pb);
  if (!rs) {
    // TODO: log error.
    return NULL;
  }

  /* Retrieve the search response. */
  rv = rc->_read(rs);
  if (!rv) {
    // TODO: log error.
    return NULL;
  }

  return rv;
}

/** \brief Get the client id for this connection to Riak.
 *
 * Check rv->mc to see if the function succeeded
 * ( \c MC_RpbGetClientIdResp) or failed ( \c MC_RpbErrorResp).
 * See \c rv->gc.resp for the \c RpbGetClientIdResp protobuf with the
 * response.
 *
 * \param rc Riak client object.
 *
 * See proto/riak_kv.proto for the fields.
 */
RiakResponse *
riak_get_client_id(RiakClient *rc)
{
  RiakSession *rs;
  RiakResponse *rv;

  /* Make and send a get client id request. */
  rs = rc->_write(rc, NULL, MC_RpbGetClientIdReq, 0, NULL);
  if (!rs) {
    // TODO: log error.
    return NULL;
  }

  /* Retrieve a get client id response. */
  rv = rc->_read(rs);
  if (!rv) {
    // TODO: log error.
    return NULL;
  }

  return rv;
}

/** \brief Set the client id for this connection to Riak.
 *
  Check rv->mc to see if the function succeeded (MC_RpbSetClientIdResp)
  or failed (MC_RpbErrorResp).  Check that rv->success is 1 to confirm
  the request succeeded.

  RiakClient *rc Riak client object.

  unsigned char *client_id The desired client_id.
 */
RiakResponse *
riak_set_client_id(RiakClient *rc, unsigned char *client_id)
{
  uint8_t *pb;
  size_t len;
  RpbSetClientIdReq req = RPB_SET_CLIENT_ID_REQ__INIT;
  RiakSession *rs;
  RiakResponse *rv;

  /* Make and send a set client id request. */
  str2pbbd(&req.client_id, client_id);
  len = rpb_set_client_id_req__get_packed_size(&req);
  pb = rc->allocator->alloc(rc->allocator->allocator_data, len);
  (void)rpb_set_client_id_req__pack(&req, pb);
  rs = rc->_write(rc, NULL, MC_RpbSetClientIdReq, len, pb);
  rc->allocator->free(rc->allocator->allocator_data, pb);
  if (!rs) {
    // TODO: log error.
    return NULL;
  }

  /* Retrieve a set client id response. */
  rv = rc->_read(rs);
  if (!rv) {
    // TODO: log error.
    return NULL;
  }

  return rv;
}

/** \brief Get Riak server info.
 *
  Check rv->mc to see if the function succeeded (MC_RpbGetServerInfoResp)
  or failed (MC_RpbErrorResp).  See rv->si.resp for the
  RpbGetServerInfoResp protobuf.

  RiakClient *rc Riak client object.

  See proto/riak_kv.proto for the fields.
 */
RiakResponse *
riak_get_server_info(RiakClient *rc)
{
  RiakSession *rs;
  RiakResponse *rv;

  /* Make and send a server info request. */
  rs = rc->_write(rc, NULL, MC_RpbGetServerInfoReq, 0, NULL);
  if (!rs) {
    // TODO: log error.
    return NULL;
  }

  /* Retrieve a server info response. */
  rv = rc->_read(rs);

  return rv;
}
