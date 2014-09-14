#ifndef RIAK_API_H
#define RIAK_API_H

#include "riak.pb-c.h"
#include "riak_dt.pb-c.h"
#include "riak_kv.pb-c.h"
#include "riak_search.pb-c.h"
#include "riak_yokozuna.pb-c.h"

/** Enum for riak message codes.
 *
 * The protobuf comms between the riak server and client uses message
 * codes to define the messages being sent and received.
 */
typedef enum _riak_mc_t {
  MC_RpbErrorResp,
  MC_RpbPingReq,
  MC_RpbPingResp,
  MC_RpbGetClientIdReq,
  MC_RpbGetClientIdResp,
  MC_RpbSetClientIdReq,
  MC_RpbSetClientIdResp,
  MC_RpbGetServerInfoReq,
  MC_RpbGetServerInfoResp,
  MC_RpbGetReq,
  MC_RpbGetResp,
  MC_RpbPutReq,
  MC_RpbPutResp,
  MC_RpbDelReq,
  MC_RpbDelResp,
  MC_RpbListBucketsReq,
  MC_RpbListBucketsResp,
  MC_RpbListKeysReq,
  MC_RpbListKeysResp,
  MC_RpbGetBucketReq,
  MC_RpbGetBucketResp,
  MC_RpbSetBucketReq,
  MC_RpbSetBucketResp,
  MC_RpbMapRedReq,
  MC_RpbMapRedResp,
  MC_RpbIndexReq,
  MC_RpbIndexResp,
  MC_RpbSearchQueryReq,
  MC_RpbSearchQueryResp,
  MC_RpbMAX,
  MC_RpbLibError
} riak_mc_t;

#define RIAK_ONE 4294967295-1
#define RIAK_QUORUM 4294967295-2
#define RIAK_ALL 4294967295-3
#define RIAK_DEFAULT 4294967295-4

#define RIAK_ACT_WRITE 1
#define RIAK_ACT_READ_HDR 2
#define RIAK_ACT_READ_PROC_HDR 3
#define RIAK_ACT_READ_PB 4
#define RIAK_ACT_FREE 5

/** \brief Data for a Riak server connection.
 *
 * This is used in the RiakClient and RiakResponse types to track
 * connections to Riak servers.
 */
struct _RiakServer {
  char *host,  ///< Host name the server this client is connected to.
       *port;  ///< Port of the server this client is connected to.
  int sd;      ///< Socket descriptor of the connection.
  int inuse;   ///< Set to one if it is being used by a streaming API.
};

typedef struct _RiakClient RiakClient;
typedef struct _RiakSession RiakSession;
typedef struct _RiakResponse RiakResponse;

/** \brief Keeps state of the Riak client.
 *
 * This structure is used to track connections to Riak servers,
 * how they're used and any errors.
 *
 * Errors are reported via the "last_*" members.
 */
struct _RiakClient {
  struct _RiakServer *servers;    ///< List of riak servers.
  int n_servers;             ///< Number of known servers.
  int current;               ///< Index to current server.
  ProtobufCAllocator *allocator;  ///< For allocating/freeing memory.
  int last_errno;            ///< Last errno value.
  int last_erract;           ///< Last error action.
  ssize_t last_errbytes;     ///< Last value of "bytes" before error.
  RiakSession *(*_write)(RiakClient *,
      RiakResponse *,
      uint8_t,
      uint32_t,
      uint8_t *);            ///< Request function.
  RiakResponse *(*_read)(RiakSession *);    ///< Response function.
};

struct _RiakSession {
  RiakClient *_rc;           ///< Associated RiakClient object.
  int server;                ///< Server index if a server was used.
                             ///  -1 if dynamic.
  int sd;                    ///< Socket for this session.
  int streaming;             ///< Only for MC_RpbIndexReq/Resp. Sigh.
};

/** \brief Data for various responses.
 *
 * All responses from riak reqests are stored in this structure.
 */
struct _RiakResponse {
  RiakSession *_rs;          ///< Associated RiakSession object.
  union {
    struct {
      char *msg;
      uint8_t mc;
    } liberr;                ///< Library error response.
    struct {
      RpbErrorResp *resp;
    } err;                   ///< Error response.
    struct {
      RpbGetClientIdResp *resp;
    } gc;                    ///< Get client id response.
    struct {
      RpbGetServerInfoResp *resp;
    } si;                    ///< Server info response.
    struct {
      RpbListBucketsResp *resp;
    } bl;                    ///< Bucket list response.
    struct {
      RpbListKeysResp *resp;
    } kl;                    ///< Key list response.
    struct {
      RpbGetBucketResp *resp;
    } bp;                    ///< Get bucket properties
    struct {
      RpbGetResp *resp;
    } g;                     ///< Get/fetch response.
    struct {
      RpbPutResp *resp;
    } p;                     ///< Put response.
    struct {
      RpbMapRedResp *resp;
    } mr;                    ///< Map reduce response.
    struct {
      RpbIndexResp *resp;
    } i;                     ///< Secondary index response.
    struct {
      RpbSearchQueryResp *resp;
    } s;                     ///< Search query response.
  };
  uint8_t mc;  ///< Message code - see enum _riak_mc_t.
  int success;  ///< Whether the expected message has been returned.
};

/* Riak API functions. */

/* API Group: Communications. */
extern RiakClient *riak_client_init(ProtobufCAllocator *allocator,
    int max_servers);
extern int riak_server_add(RiakClient *rc, char *host, char *port);
extern int riak_server_del(RiakClient *rc, char *host, char *port);
extern int riak_servers_known(RiakClient *rc);
extern int riak_servers_active(RiakClient *rc);
extern void riak_servers_disconnect(RiakClient *rc);

/* All functions below use a RiakResponse type. It holds a response from
 * riak API call. */
extern void riak_response_free(RiakClient *rc, RiakResponse *rv);
extern void riak_response_only_free(RiakClient *rc, RiakResponse *rv);
extern const char *riak_mc2str(uint8_t mc);

/* API Group: Bucket Operations. */
extern RiakResponse *riak_list_buckets(RiakClient *rc);
extern RiakResponse *riak_list_keys(RiakClient *rc, RiakResponse *rv,
    unsigned char *bucket);
extern RiakResponse *riak_get_bucket_props(RiakClient *rc,
    unsigned char *bucket);
extern RiakResponse *riak_set_bucket_props(RiakClient *rc,
    unsigned char *bucket, RpbBucketProps *props);

/* API Group: Object/Key Operations. */
extern RiakResponse *riak_fetch_object_full(RiakClient *rc,
    unsigned char *bucket, ProtobufCBinaryData *key, RpbGetReq *req);
extern RiakResponse *riak_store_object_full(RiakClient *rc, RpbPutReq *req);
extern RiakResponse *riak_delete_object_full(RiakClient *rc,
    RpbDelReq *req);

/* API Group: Query Operations. */
extern RiakResponse *riak_map_reduce(RiakClient *rc,
    RiakResponse *rv, unsigned char *request, unsigned char *content_type);
extern RiakResponse *riak_secondary_indexes(RiakClient *rc,
    RiakResponse *rv, RpbIndexReq *req);
extern RiakResponse *riak_search(RiakClient *rc, RpbSearchQueryReq *req);

/* API Group: Server Operations */
extern RiakResponse *riak_ping(RiakClient *rc);
extern RiakResponse *riak_get_client_id(RiakClient *rc);
extern RiakResponse *riak_set_client_id(RiakClient *rc,
    unsigned char *client_id);
extern RiakResponse *riak_get_server_info(RiakClient *rc);

#endif /* RIAK_API_H */
