#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef enum _kv_kind_t {
  KVK_RIAK,
  KVK_MAX
} kv_kind_t;

typedef struct _kv_uri_t {
  kv_kind_t kind;
  union {
    struct {
      char *bucket;
      char *bucket_len;
      char *key;
      char *key_len;
    } riak;
  };
} kv_uri_t;

kv_uri_t *
parse_uri(char *uri)
{
  kv_uri_t *kv;

  kv = malloc(sizeof(kv_uri_t));
  return kv;
}
