#ifndef RIAK_DEBUG_H
#define RIAK_DEBUG_H

#include "riakccs/api.h"
#include <google/protobuf-c/protobuf-c.h>

extern void escape_print(uint32_t len, uint8_t *data);
extern void dump_msg(int indent, char *msg_name, uint8_t *hdr,
    size_t len, uint8_t *msg);
extern void dump_ProtobufCBinaryData(int indent, char *pbd_name,
    ProtobufCBinaryData *pbd, size_t len);
extern void dump_RpbLink(int indent, char *rl_name, RpbLink *rl, size_t len);
extern void dump_RpbPair(int indent, char *rp_name, RpbPair *rp, size_t len);
extern void dump_RpbContent(int indent, char *rcont_name,
    RpbContent *rcont, size_t len);

#endif /* RIAK_DEBUG_H */
