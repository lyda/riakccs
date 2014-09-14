#ifndef PB_H
#define PB_H

#include <google/protobuf-c/protobuf-c.h>

extern void str2pbbd(ProtobufCBinaryData *dst, unsigned char *src);
extern ProtobufCAllocator pbc_sys_allocator;

#endif /* PB_H */
