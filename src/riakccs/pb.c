/** \file
 *
 * \brief Protobuf utility functions.
 *
 * Functions for manipulating various protobuf data structures.
 */

#include <stdio.h>
#include <string.h>
#include <google/protobuf-c/protobuf-c.h>

/** \brief Copy C string into protobuf string type.
 *
 * str2pbbd puts a string (src) into a ProtobufCBinaryData (dst).
 * Note that the function does not allocate any space.
 *
 * \param dst Destination protobuf string.
 * \param src Source string.
 */
void
str2pbbd(ProtobufCBinaryData *dst, unsigned char *src)
{
  dst->data = src;
  dst->len = strlen((char *)src);
}
