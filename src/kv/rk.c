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
#include <getopt.h>

#include "rk_parse.h"
#include "riakccs/api.h"
#include "riakccs/pb.h"

static void action_ls(action_t *action, RiakClient *rc);
static void action_add(action_t *action, RiakClient *rc);
static void action_cat(action_t *action, RiakClient *rc);
static void action_rm(action_t *action, RiakClient *rc);
static void action_prop(action_t *action, RiakClient *rc);
static void action_map(action_t *action, RiakClient *rc);
static void action_grep(action_t *action, RiakClient *rc);

#define SDEF(E, T) [(E)] = (T)
static void (*subcmd_acts[])(action_t *, RiakClient *) = {
  SDEF(RK_SC_LS, action_ls),
  SDEF(RK_SC_ADD, action_add),
  SDEF(RK_SC_CAT, action_cat),
  SDEF(RK_SC_RM, action_rm),
  SDEF(RK_SC_PROP, action_prop),
  SDEF(RK_SC_MAP, action_map),
  SDEF(RK_SC_GREP, action_grep)
};
#undef SDEF

/* Mark this function as one that exits to remove false positives
 * from static analysis.
 */
void usage_rv(RiakResponse *rv, const char *usage_msg)
  __attribute__((noreturn));

void
usage_rv(RiakResponse *rv, const char *usage_msg)
{
  if (rv) {
    switch (rv->mc) {
      case MC_RpbErrorResp:
        printf("ERROR: Riak server error %d: %.*s\n", rv->err.resp->errcode,
            (int)rv->err.resp->errmsg.len, rv->err.resp->errmsg.data);
        break;
      case MC_RpbLibError:
        printf("ERROR: Riak lib error: %s\n", rv->liberr.msg);
        break;
      default:
        printf("ERROR: Riak unexpected mc error %d: %s\n", rv->liberr.mc,
            rv->liberr.msg);
        break;
    }
  } else {
    printf("ERROR: Riak lib error: memory allocation error.\n");
  }
  usage((char *)usage_msg);
}

RiakClient *
server_connect(action_t *action)
{
  RiakClient *rc = NULL;
  int i;

  if (action->n_urls == 0) {
    usage("Need to specify hosts with -u or RK_SERVERS.");
  }
  rc = riak_client_init(NULL, action->n_urls);
  for (i = 0; i < action->n_urls; i++) {
    riak_server_add(rc, action->hosts[i], action->ports[i]);
    if (action->verbose) {
      printf("Connecting to: %s:%s\n", action->hosts[i], action->ports[i]);
    }
  }

  return rc;
}

static void
action_ls(action_t *action, RiakClient *rc)
{
  int i, j;
  RiakResponse *rv = NULL;

  if (action->ls.n_buckets) {
    for (i = 0; i < action->ls.n_buckets; i++) {
      if (action->ls.n_buckets > 1) {
        printf("%s:\n", action->ls.buckets[i]);
      }
      rv = NULL;
      do {
        rv = riak_list_keys(rc, rv, action->ls.buckets[i]);
        if (!rv || !rv->success) {
          usage_rv(rv, "ls: Comms error.");
        }
        for (j = 0; j < rv->kl.resp->n_keys; j++) {
          if (action->ls.hex) {
            int i;

            for (i = 0; i < rv->kl.resp->keys[j].len; i++) {
              printf("%02hhx", rv->kl.resp->keys[j].data[i]);
            }
            printf("\n");
          } else {
            printf("%.*s\n", (int)rv->kl.resp->keys[j].len,
                rv->kl.resp->keys[j].data);
          }
          if (action->ls.verbose) {
            RiakResponse *rv_get = NULL;
            RpbGetReq req = RPB_GET_REQ__INIT;

            req.has_head = 1;
            req.head = 1;
            rv_get = riak_fetch_object_full(rc, action->ls.buckets[i],
                &rv->kl.resp->keys[j], &req);
            if (rv_get && rv_get->g.resp->n_content > 0) {
              int i;

              if (rv_get->g.resp->content[0]->n_usermeta > 0) {
                printf("  usermeta: ");
                for (i = 0; i < rv_get->g.resp->content[0]->n_usermeta
                    && printf(", "); i++) {
                  if (rv_get->g.resp->content[0]->usermeta[i]->has_value) {
                    printf("{%.*s: ",
                        (int)rv_get->g.resp->content[0]->usermeta[i]->key.len,
                        rv_get->g.resp->content[0]->usermeta[i]->key.data);
                    escape_print(
                        rv_get->g.resp->content[0]->usermeta[i]->value.len,
                        rv_get->g.resp->content[0]->usermeta[i]->value.data);
                    printf("}\n");
                  } else {
                    printf("{%.*s}",
                        (int)rv_get->g.resp->content[0]->usermeta[i]->key.len,
                        rv_get->g.resp->content[0]->usermeta[i]->key.data);
                  }
                }
                printf("\n");
              }
            }
            if (rv_get) {
              riak_response_free(rc, rv_get);
            }
          }
        }
      } while (!rv->kl.resp->has_done && !rv->kl.resp->done);
    }
  } else {
    rv = riak_list_buckets(rc);
    if (!rv || !rv->success) {
      usage_rv(rv, "ls: Comms error.");
    }
    if (!rv) {
      usage("Memory error allocating RiakResponse (riak_list_buckets).");
    }
    for (i = 0; i < rv->bl.resp->n_buckets; i++) {
      printf("%.*s\n", (int)rv->bl.resp->buckets[i].len,
          rv->bl.resp->buckets[i].data);
    }
  }
  if (rv) {
    riak_response_free(rc, rv);
  }
}

static void
action_add(action_t *action, RiakClient *rc)
{
  RiakResponse *rv;
  RpbPutReq req = RPB_PUT_REQ__INIT;
  RpbContent content = RPB_CONTENT__INIT;

  str2pbbd(&req.bucket, action->add.bucket);
  req.has_key = 1;
  req.key.len = action->add.key.len;
  req.key.data = action->add.key.data;
  req.content = &content;
  content.value.len = action->add.value.len;
  content.value.data = action->add.value.data;
  content.has_content_type = 1;
  str2pbbd(&content.content_type, "plain/text");

  rv = riak_store_object_full(rc, &req);
  if (!rv || !rv->success) {
    usage_rv(rv, "add: Comms error.");
  }
  riak_response_free(rc, rv);
}

static void
action_cat(action_t *action, RiakClient *rc)
{
  RiakResponse *rv;
  RpbGetReq req = RPB_GET_REQ__INIT;

  rv = riak_fetch_object_full(rc, action->cat.bucket, &action->cat.key,
      &req);
  if (!rv || !rv->success) {
    usage_rv(rv, "cat: Comms error.");
  }
  if (rv->g.resp->n_content) {
    int i;
    uint8_t c;

    if (action->cat.human) {
      for (i = 0; i < rv->g.resp->content[0]->value.len; i++) {
        c = rv->g.resp->content[0]->value.data[i];
        if (isprint((int)c) || isspace((int)c)) {
          printf("%c", (unsigned char)c);
        } else {
          printf("\\x%02x", (unsigned char)c);
        }
      }
    } else {
      fwrite(rv->g.resp->content[0]->value.data,
          rv->g.resp->content[0]->value.len, 1, stdout);
    }
  }
  riak_response_free(rc, rv);
}

static void
action_rm(action_t *action, RiakClient *rc)
{
  RiakResponse *rv;
  RpbDelReq req = RPB_DEL_REQ__INIT;

  if (action->rm.key.len > 0) {
    req.bucket.data = action->rm.bucket;
    req.bucket.len = strlen(action->rm.bucket);
    req.key.data = action->rm.key.data;
    req.key.len = action->rm.key.len;
    rv = riak_delete_object_full(rc, &req);
    if (!rv || !rv->success) {
      usage_rv(rv, "rm: Comms error.");
    }
    riak_response_free(rc, rv);
  } else {
    if (action->rm.recursive && action->rm.force) {
      RiakResponse *rv_rm;
      int i;

      rv = NULL;
      do {
        rv = riak_list_keys(rc, rv, action->rm.bucket);
        if (!rv || !rv->success) {
          usage_rv(rv, "rm: Comms error.");
        }
        req.bucket.data = action->rm.bucket;
        req.bucket.len = strlen(action->rm.bucket);
        for (i = 0; i < rv->kl.resp->n_keys; i++) {
          req.key.data = rv->kl.resp->keys[i].data;
          req.key.len = rv->kl.resp->keys[i].len;
          rv_rm = riak_delete_object_full(rc, &req);
          if (!rv_rm->success) {
            usage_rv(rv_rm, "rm: Comms error.");
          }
          riak_response_free(rc, rv_rm);
        }
      } while (!rv->kl.resp->has_done && !rv->kl.resp->done);
    } else {
      usage("rm: Must specify -rf if you want to remove a bucket.");
    }
  }
}

static void
action_prop(action_t *action, RiakClient *rc)
{
  printf("Not implemented.\n");
}

static void
action_map(action_t *action, RiakClient *rc)
{
  RiakResponse *rv;

  rv = riak_map_reduce(rc, NULL, action->map.expression,
      action->map.expr_type);
  if (!rv || !rv->success) {
    usage_rv(rv, "map: Comms error.");
  }
  while (rv && !(rv->mr.resp->has_done && rv->mr.resp->done)) {
    if (rv->mr.resp->has_phase) {
      printf("Map reduce phase %d\n", rv->mr.resp->phase);
    }
    if (rv->mr.resp->has_response) {
      printf("%.*s\n", (int)rv->mr.resp->response.len,
          rv->mr.resp->response.data);
    }
    rv = riak_map_reduce(rc, rv, action->map.expression,
        action->map.expr_type);
    if (!rv || !rv->success) {
      usage_rv(rv, "map: Comms error.");
    }
  }
  riak_response_free(rc, rv);
}

static void
action_grep(action_t *action, RiakClient *rc)
{
  RiakResponse *rv;
  int len, i;
  char *expr_type = "application/json";
  char *expr_fmt =
    "{\"inputs\":%s,"
     "\"query\":[{\"map\":{\"language\":\"javascript\","
     "\"source\":\"function(riakObject) {"
                     "var val = riakObject.values[0].data.match(/%s/g);"
                     "return [[riakObject.key, (val? val.length: 0 )]];"
    "}\"}}]}";
  char *expression, *inputs, *regex;

  /* TODO: clean up the regex - escape " and /'s at least. */
  regex = strdup(action->grep.regex);

  len = 0;
  if (!action->grep.n_keys) {
    len = strlen(action->grep.bucket) + 5;
    inputs = malloc(len);
    snprintf(inputs, len, "\"%s\"", action->grep.bucket);
    inputs[len - 1] = '\0';
  } else {
    int cur_off = 1;

    for (i = 0; i < action->grep.n_keys; i++) {
      len += strlen(action->grep.bucket) + strlen(action->grep.keys[i]) + 20;
    }
    len += 3;  // For chars [, ] and \0.
    inputs = malloc(len);
    strcpy(inputs, "[");
    for (i = 0; i < action->grep.n_keys; i++) {
      cur_off += snprintf(inputs + cur_off,
          strlen(action->grep.bucket) + strlen(action->grep.keys[i]) + 20,
          "[\"%s\",\"%s\"],", action->grep.bucket, action->grep.keys[i]);
    }
    inputs[cur_off - 1] = ']';  // Replace trailing , with a ].
    inputs[len - 1] = '\0';
  }

  len = strlen(expr_fmt);
  len += strlen(inputs);
  len += strlen(regex);
  expression = malloc(len);
  snprintf(expression, len, expr_fmt, inputs, regex);
  expression[len - 1] = '\0';
  printf("expression: %s\n", expression);
  free(inputs);
  free(regex);

  rv = riak_map_reduce(rc, NULL, expression, expr_type);
  if (!rv || !rv->success) {
    usage_rv(rv, "grep: Comms error.");
  }
  while (rv && !(rv->mr.resp->has_done && rv->mr.resp->done)) {
    if (rv->mr.resp->has_phase) {
      printf("Map reduce phase %d\n", rv->mr.resp->phase);
    }
    if (rv->mr.resp->has_response) {
      printf("%.*s\n", (int)rv->mr.resp->response.len,
          rv->mr.resp->response.data);
    }
    rv = riak_map_reduce(rc, rv, expression, expr_type);
    if (!rv || !rv->success) {
      usage_rv(rv, "grep: Comms error.");
    }
  }

  free(expression);
  riak_response_free(rc, rv);
}

int
main(int argc, char *argv[])
{
  char *cmd;
  RiakClient *rc;
  int i;
  action_t *action;

  action = parse_commandline(argc, argv);
  if (action->debug) {
    dump_action(action);
  }

  rc = server_connect(action);
  subcmd_acts[action->subcommand](action, rc);
  if (rc) {
    riak_servers_disconnect(rc);
  }

  return 0;
}
