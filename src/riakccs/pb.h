#ifndef PB_H
#define PB_H

#include <google/protobuf-c/protobuf-c.h>

extern void str2pbbd(ProtobufCBinaryData *dst, unsigned char *src);

#endif /* PB_H */
