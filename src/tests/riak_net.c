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
#include <unistd.h>
#include <time.h>

#include "riak_dt.pb-c.h"
#include "riak_kv.pb-c.h"
#include "riak.pb-c.h"
#include "riak_search.pb-c.h"
#include "riak_yokozuna.pb-c.h"
#include "riakccs/api.h"
#include "riakccs/pb.h"
#include "riakccs/debug.h"

#include <check.h>

/* Used for test_malloc to set "random" failures. */
static unsigned int seed;
static int malloc_fail = 0;
static char *riak_host = NULL;
static char *riak_port = NULL;
static char *riak_host2 = NULL;
static char *riak_port2 = NULL;

static void *
broken_alloc(void *allocator_data, size_t size)
{
  void *rv = NULL;
  (void)allocator_data;
  if (size != 0)
    if (rand_r(&seed) % 10)
      rv = malloc (size);
    else
      malloc_fail++;
  return rv;
}

static void
broken_free(void *allocator_data, void *data)
{
  (void)allocator_data;
  if (data)
    free(data);
}

ProtobufCAllocator broken_allocator = {
  broken_alloc, broken_free, NULL, 8192, NULL
};

START_TEST(test_riak_connect)
{
  RiakClient *rc;
  int count;

  rc = riak_client_init(NULL, 3);
  riak_server_add(rc, riak_host, riak_port);
  count = riak_servers_active(rc);
  ck_assert_int_eq(count, 1);
  count = riak_server_add(rc, riak_host2, riak_port2);
  ck_assert_int_eq(count, 1);
  count = riak_servers_active(rc);
  ck_assert_int_eq(count, 2);
  count = riak_server_del(rc, riak_host, riak_port);
  ck_assert_int_eq(count, 1);
  count = riak_servers_active(rc);
  ck_assert_int_eq(count, 1);

  riak_servers_disconnect(rc);
}
END_TEST

START_TEST(test_riak_bad_initial_connect)
{
  RiakClient *rc;
  int count;

  rc = riak_client_init(NULL, 3);
  riak_server_add(rc, "localhost", "808");
  count = riak_servers_active(rc);
  ck_assert_int_eq(count, 0);
  count = riak_server_add(rc, riak_host2, riak_port2);
  ck_assert_int_eq(count, 1);
  count = riak_servers_active(rc);
  ck_assert_int_eq(count, 1);
  count = riak_server_del(rc, riak_host, riak_port);
  ck_assert_int_eq(count, 0);
  count = riak_servers_active(rc);
  ck_assert_int_eq(count, 1);

  riak_servers_disconnect(rc);
}
END_TEST

Suite *
test_suite_riak_conn(void)
{
  Suite *s = suite_create("Riak C API - Connect");
  TCase *tc = tcase_create("Connect");

  /* Networked tests */
  tcase_add_test(tc, test_riak_connect);
  tcase_add_test(tc, test_riak_bad_initial_connect);
  suite_add_tcase(s, tc);

  return s;
}

START_TEST(test_riak_ping)
{
  RiakClient *rc;
  RiakResponse *rv;

  rc = riak_client_init(NULL, 1);
  riak_server_add(rc, riak_host, riak_port);

  /* ping request. */
  rv = riak_ping(rc);
  ck_assert_int_eq(rv->success, 1);
  ck_assert_int_eq(rv->mc, MC_RpbPingResp);
  riak_response_free(rc, rv);

  riak_servers_disconnect(rc);
}
END_TEST

START_TEST(test_riak_client_id)
{
  RiakClient *rc;
  RiakResponse *rv;
  char *client_id = "kitten";

  rc = riak_client_init(NULL, 1);
  riak_server_add(rc, riak_host, riak_port);

  /* set client id request. */
  rv = riak_set_client_id(rc, client_id);
  ck_assert_int_eq(rv->success, 1);
  ck_assert_msg(rv->mc == MC_RpbSetClientIdResp,
      "Expected rv->mc to be 'MC_RpbSetClientIdResp,' instead got '%s'",
      riak_mc2str(rv->mc));
  riak_response_free(rc, rv);
  rv = riak_get_client_id(rc);
  ck_assert_int_eq(rv->success, 1);
  ck_assert_msg(rv->mc == MC_RpbGetClientIdResp,
      "Expected rv->mc to be 'MC_RpbGetClientIdResp,' instead got '%s'",
      riak_mc2str(rv->mc));
  ck_assert_msg(rv->gc.resp->client_id.len == strlen(client_id),
      "Expected 'kitten' got '%.*s'(%d) instead.",
      rv->gc.resp->client_id.len,
      rv->gc.resp->client_id.data,
      rv->gc.resp->client_id.len);
  ck_assert_msg(
      strncmp(rv->gc.resp->client_id.data, client_id,
        rv->gc.resp->client_id.len) == 0,
      "Expected client_id to be 'kitten'");
  riak_response_free(rc, rv);

  riak_servers_disconnect(rc);
}
END_TEST

START_TEST(test_riak_server_info)
{
  RiakClient *rc;
  RiakResponse *rv;

  rc = riak_client_init(NULL, 1);
  riak_server_add(rc, riak_host, riak_port);

  /* get server info request. */
  rv = riak_get_server_info(rc);
  ck_assert_int_eq(rv->success, 1);
  ck_assert_msg(rv->mc == MC_RpbGetServerInfoResp,
      "Expected rv->mc to be 'MC_RpbGetServerInfoResp,' instead got '%s'",
      riak_mc2str(rv->mc));
  riak_response_free(rc, rv);

  riak_servers_disconnect(rc);
}
END_TEST

START_TEST(test_riak_list_buckets)
{
  RiakClient *rc;
  RiakResponse *rv;
  int i, found = 0;

  rc = riak_client_init(NULL, 1);
  riak_server_add(rc, riak_host, riak_port);

  /* list buckets request. */
  rv = riak_list_buckets(rc);
  ck_assert_int_eq(rv->success, 1);
  ck_assert_msg(rv->mc == MC_RpbListBucketsResp,
      "Expected rv->mc to be 'MC_RpbListBucketsResp,' instead got '%s'",
      riak_mc2str(rv->mc));
  // TODO(lyda): For a full test, create a bucket and then look for it.
  riak_response_free(rc, rv);

  riak_servers_disconnect(rc);
}
END_TEST

START_TEST(test_riak_list_keys)
{
  RiakClient *rc;
  RiakResponse *rv = NULL;
  int i, keyct = 0;

  rc = riak_client_init(NULL, 1);
  riak_server_add(rc, riak_host, riak_port);

  /* list keys request. */
  do {
    rv = riak_list_keys(rc, rv, "code");
    ck_assert_int_eq(rv->success, 1);
    ck_assert_msg(rv->mc == MC_RpbListKeysResp,
        "Expected rv->mc to be 'MC_RpbListBucketsResp,' instead got '%s'",
        riak_mc2str(rv->mc));
    // TODO(lyda): For a full test, create a bucket and a key and then
    //             look for the key.
  } while (!rv->kl.resp->has_done && !rv->kl.resp->done);

  riak_response_free(rc, rv);

  riak_servers_disconnect(rc);
}
END_TEST

START_TEST(test_riak_bucket_props)
{
  RiakClient *rc;
  RiakResponse *rv;
  uint32_t n_val;
  RpbBucketProps props = RPB_BUCKET_PROPS__INIT;

  rc = riak_client_init(NULL, 1);
  riak_server_add(rc, riak_host, riak_port);

  /* get bucket props request. */
  rv = riak_get_bucket_props(rc, "test_empty_bucket");
  n_val = rv->bp.resp->props->n_val;
  ck_assert_int_eq(rv->success, 1);
  ck_assert_msg(rv->mc == MC_RpbGetBucketResp,
      "Expected rv->mc to be 'MC_RpbGetBucketResp,' instead got '%s'",
      riak_mc2str(rv->mc));
  riak_response_free(rc, rv);

  props.has_n_val = 1;
  if (n_val == 3) {
    props.n_val = 5;
  } else {
    props.n_val = 3;
  }

  /* set bucket props request. */
  rv = riak_set_bucket_props(rc, "test_empty_bucket", &props);
  ck_assert_int_eq(rv->success, 1);
  ck_assert_msg(rv->mc == MC_RpbSetBucketResp,
      "Expected rv->mc to be 'MC_RpbSetBucketResp,' instead got '%s'",
      riak_mc2str(rv->mc));
  riak_response_free(rc, rv);

  /* get bucket props request. */
  rv = riak_get_bucket_props(rc, "test_empty_bucket");
  ck_assert_int_eq(rv->success, 1);
  ck_assert_msg(rv->mc == MC_RpbGetBucketResp,
      "Expected rv->mc to be 'MC_RpbGetBucketResp,' instead got '%s'",
      riak_mc2str(rv->mc));
  ck_assert_msg(props.n_val == rv->bp.resp->props->n_val,
      "Expected rv->bp.resp->props->n_val to be %d instead got %d",
      props.n_val, rv->bp.resp->props->n_val);
  riak_response_free(rc, rv);

  riak_servers_disconnect(rc);
}
END_TEST

START_TEST(test_riak_get_put)
{
  RiakClient *rc;
  RiakResponse *rv;
  char label[20];
  int i;
  uint32_t n_content;
  RpbPutReq put_req = RPB_PUT_REQ__INIT;
  RpbContent content = RPB_CONTENT__INIT;
  char *bucket = "test",
       *value = "Look! Das kitteh!";
  ProtobufCBinaryData key = { 16, "get_put_test_key" };
  RpbGetReq get_req = RPB_GET_REQ__INIT;
  RpbDelReq del_req = RPB_DEL_REQ__INIT;

  rc = riak_client_init(NULL, 1);
  riak_server_add(rc, riak_host, riak_port);

  /* Delete test bucket/key (success unimportant). */
  del_req.bucket.data = bucket;
  del_req.bucket.len = strlen(bucket);
  del_req.key.data = key.data;
  del_req.key.len = key.len;
  rv = riak_delete_object_full(rc, &del_req);
  riak_response_free(rc, rv);

  /* put object request. */
  str2pbbd(&put_req.bucket, bucket);
  put_req.has_key = 1;
  put_req.key.data = key.data;
  put_req.key.len = key.len;
  put_req.content = &content;
  str2pbbd(&content.value, value);
  content.has_content_type = 1;
  str2pbbd(&content.content_type, "plain/text");
  rv = riak_store_object_full(rc, &put_req);
  ck_assert_int_eq(rv->success, 1);
  ck_assert_msg(rv->mc == MC_RpbPutResp,
      "Expected rv->mc to be 'MC_RpbPutBucketResp,' instead got '%s'",
      riak_mc2str(rv->mc));
  riak_response_free(rc, rv);

  /* get object request. */
  rv = riak_fetch_object_full(rc, bucket, &key, &get_req);
  n_content = rv->g.resp->n_content;
  ck_assert_int_eq(rv->success, 1);
  ck_assert_msg(rv->mc == MC_RpbGetResp,
      "Expected rv->mc to be 'MC_RpbGetBucketResp,' instead got '%s'",
      riak_mc2str(rv->mc));
  ck_assert_msg(rv->g.resp->n_content == 1,
      "Expected rv->g.resp->n_content == 1, instead is %zd",
      rv->g.resp->n_content);
  ck_assert_msg(rv->g.resp->content[0]->value.len == strlen(value),
      "Expected content[0]->value.len == %d, instead is %zd", strlen(value),
      rv->g.resp->content[0]->value.len);
  ck_assert_msg(
      strncmp(rv->g.resp->content[0]->value.data, value,
        rv->g.resp->content[0]->value.len) == 0,
      "Expected content[0]->value to be '%s'", value);
  riak_response_free(rc, rv);

  /* Delete test bucket/key (success checked). */
  rv = riak_delete_object_full(rc, &del_req);
  ck_assert_int_eq(rv->success, 1);
  riak_response_free(rc, rv);

  riak_servers_disconnect(rc);
}
END_TEST

Suite *
test_suite_riak_net(void)
{
  Suite *s = suite_create("Riak C API - API");
  TCase *tc = tcase_create("API");

  /* Networked tests */
  tcase_add_test(tc, test_riak_ping);
  tcase_add_test(tc, test_riak_client_id);
  tcase_add_test(tc, test_riak_server_info);
  tcase_add_test(tc, test_riak_list_buckets);
  tcase_add_test(tc, test_riak_list_keys);
  tcase_add_test(tc, test_riak_bucket_props);
  tcase_add_test(tc, test_riak_get_put);
  suite_add_tcase(s, tc);

  return s;
}

int
main(int argc, char *argv[])
{
  RiakClient *rc;
  int number_failed, exit_code = EXIT_SUCCESS;
  Suite *s_api = test_suite_riak_net();
  SRunner *sr_api = srunner_create(s_api);
  Suite *s_conn = test_suite_riak_conn();
  SRunner *sr_conn = srunner_create(s_conn);
  enum print_output print_mode = CK_VERBOSE;

  /* Set verbosity. By default be verbose, but let user override. */
  if (getenv("CK_VERBOSITY") && strcmp(getenv("CK_VERBOSITY"), "verbose"))
    print_mode = CK_ENV;

  /* Set the seed. */
  if (argc == 2) {
    seed = (unsigned int)atoi(argv[1]);
    if (print_mode == CK_VERBOSE)
      printf("Seed: %u (manually set on command line)\n", seed);
  } else if (getenv("CK_SEED")) {
    seed = (unsigned int)atoi(getenv("CK_SEED"));
    if (print_mode == CK_VERBOSE)
      printf("Seed: %u (manually set with CK_SEED)\n", seed);
  } else {
    time_t time_now;
    struct tm now;

    time_now = time(NULL);
    localtime_r(&time_now, &now);
    seed = (unsigned int)now.tm_yday;
    if (print_mode == CK_VERBOSE)
      printf("Seed: %u (autoset by day of year - override with CK_SEED)\n",
          seed);
  }

  riak_host = getenv("RIAK_HOST1");
  riak_port = getenv("RIAK_PORT1");
  riak_host2 = getenv("RIAK_HOST2");
  riak_port2 = getenv("RIAK_PORT2");
  if (!riak_host || !riak_port_str || !riak_host2 || !riak_port_str2) {
    printf("RIAK_HOST{1,2} and/or RIAK_PORT{1,2} not supplied.  Bailing.\n");
    exit(77);
  }

  /* Try and connect - exit(77) means SKIP for test framework. */
  rc = riak_client_init(NULL, 1);
  riak_server_add(rc, riak_host, riak_port);
  if (riak_servers_active(rc) < 1)
    exit(77);
  riak_servers_disconnect(rc);

  /* Run tests related to connecting to the server. */
  srunner_run_all(sr_conn, print_mode);
  number_failed = srunner_ntests_failed(sr_conn);
  srunner_free(sr_conn);

  /* Run api tests. */
  srunner_run_all(sr_api, print_mode);
  number_failed += srunner_ntests_failed(sr_api);
  srunner_free(sr_api);

  if (number_failed > 0)
    exit_code = EXIT_FAILURE;
  return exit_code;
}
