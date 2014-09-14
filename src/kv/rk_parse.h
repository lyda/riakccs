#ifndef RK_PARSE_H
#define RK_PARSE_H

#include <google/protobuf-c/protobuf-c.h>

typedef enum _subcommand_t {
  RK_SC_LS,
  RK_SC_ADD,
  RK_SC_CAT,
  RK_SC_RM,
  RK_SC_PROP,
  RK_SC_MAP,
  RK_SC_GREP,
  RK_SC_HELP,
  RK_SC_MAX
} subcommand_t;

typedef struct _action_t {
  int n_urls;
  char **hosts;
  char **ports;
  int verbose;
  int debug;
  subcommand_t subcommand;
  union {
    struct {
      int verbose;
      int hex;
      int n_buckets;
      char **buckets;
    } ls;
    struct {
      int human;
      int number;
      char *bucket;
      ProtobufCBinaryData key;
    } cat;
    struct {
      char *filename;
      char *bucket;
      ProtobufCBinaryData key;
      ProtobufCBinaryData value;
    } add;
    struct {
      int recursive;
      int force;
      char *bucket;
      ProtobufCBinaryData key;
    } rm;
    struct {
      char *bucket;
    } prop;
    struct {
      char *expr_type;
      char *expression;
    } map;
    struct {
      int just_keys;
      char *regex;
      char *bucket;
      int n_keys;
      char **keys;
    } grep;
  };
} action_t;


void dump_action(action_t *action);
void usage(char *message) __attribute__((__noreturn__));
action_t * parse_commandline(int argc, char **argv);

#endif /* RK_PARSE_H */
