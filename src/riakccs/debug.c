/** \file
 *
 * \brief Debug routines.
 *
 * Useful functions for debugging.
 */
#include <arpa/inet.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <execinfo.h>
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
#endif /* S_SPLINT_S */

#include "riakccs/debug.h"


/** \brief Return the minimum non-zero number.
 *
 * \param a First number.
 * \param b Second number.
 */
uint32_t
min_if_a_not_zero(uint32_t a, uint32_t b)
{
  if (a)
    return a < b? a: b;
  else
    return b;
}

/** \brief Dump the stack frame.
 *
 */
void
show_stackframe() {
  void *trace[16];
  char **messages = NULL;
  int i, trace_size = 0;

  trace_size = backtrace(trace, 16);
  messages = backtrace_symbols(trace, trace_size);
  printf("[bt] Execution path:\n");
  for (i = 0; i < trace_size; i++)
    printf("[bt %d] %s\n", i, messages[i]);
}

/** \brief Print a string, escaping non-printable chars.
 *
 * \param len Max number of chars to print.
 * \param data Data to print.
 */
void
escape_print(uint32_t len, uint8_t *data)
{
  uint32_t i;

  for (i = 0; i < len; i++) {
    if (isprint((int)data[i]) || isspace((int)data[i])) {
      printf("%c", (int)data[i]);
    } else {
      printf("\\x%02x", (unsigned int)data[i]);
    }
  }
}

/** \brief Dump the riak protocol header and data block.
 *
 * \param indent Number of chars to indent.
 * \param msg_name Name of the message being dumped.
 * \param hdr The header to be dumped.
 * \param len The max length of any string to print.
 * \param msg The data block to be dumped.
 */
void
dump_msg(int indent, char *msg_name, uint8_t *hdr, size_t len,
    uint8_t *msg)
{
  int i;

  printf("%*s%s: ", indent, "", msg_name);
  for (i = 0; i < 5; i++) {
    printf("%hhx ", hdr[i]);
  }
  escape_print(len, msg);
  printf("\n");
}

/** \brief Dump protobuf strings.
 *
 * \param indent Number of chars to indent.
 * \param pbd_name Name of the var being dumped.
 * \param pbd The var to be dumped.
 * \param len The max length of any string to print.
 */
void
dump_ProtobufCBinaryData(int indent, char *pbd_name,
    ProtobufCBinaryData *pbd, size_t len)
{
  printf("%*s%s[%zd]: ", indent, "", pbd_name, pbd->len);
  escape_print(min_if_a_not_zero(len, pbd->len), pbd->data);
  printf("\n");
}

/** \brief Dump riak link structures.
 *
 * \param indent Number of chars to indent.
 * \param rl_name Name of the var being dumped.
 * \param rl The var to be dumped.
 * \param len The max length of any string to print.
 */
void
dump_RpbLink(int indent, char *rl_name, RpbLink *rl, size_t len)
{
  if (rl) {
    printf("%*srl name: %s\n", indent, "", rl_name);
    dump_ProtobufCBinaryData(indent + 2, "bucket", &rl->bucket, len);
    dump_ProtobufCBinaryData(indent + 2, "key", &rl->key, len);
    dump_ProtobufCBinaryData(indent + 2, "tag", &rl->tag, len);
  } else {
    printf("%*srl name: %s (empty)\n", indent, "", rl_name);
  }
}

/** \brief Dump riak pair structures.
 *
 * \param indent Number of chars to indent.
 * \param rp_name Name of the var being dumped.
 * \param rp The var to be dumped.
 * \param len The max length of any string to print.
 */
void
dump_RpbPair(int indent, char *rp_name, RpbPair *rp, size_t len)
{
  if (rp) {
    printf("%*srp name: %s\n", indent, "", rp_name);
    dump_ProtobufCBinaryData(indent + 2, "key", &rp->key, len);
    dump_ProtobufCBinaryData(indent + 2, "tag", &rp->value, len);
  } else {
    printf("%*srp name: %s (empty)\n", indent, "", rp_name);
  }
}

/** \brief Dump RpbContent data types.
 *
 * \param indent Number of chars to indent.
 * \param rcont_name Name of the var being dumped.
 * \param rcont The var to be dumped.
 * \param len The max length of any string to print.
 */
void
dump_RpbContent(int indent, char *rcont_name, RpbContent *rcont, size_t len)
{
  size_t i;
  char label[80];

  printf("%*srcont name: %s\n", indent, "", rcont_name);
  dump_ProtobufCBinaryData(indent + 2, "value", &rcont->value, len);
  dump_ProtobufCBinaryData(indent + 2, "content_type",
      &rcont->content_type, len);
  dump_ProtobufCBinaryData(indent + 2, "charset", &rcont->charset, len);
  dump_ProtobufCBinaryData(indent + 2, "content_encoding",
      &rcont->content_encoding, len);
  dump_ProtobufCBinaryData(indent + 2, "vtag", &rcont->vtag, len);
  printf("%*s  links len: %zd\n", indent, "", rcont->n_links);
  for (i = 0; i < rcont->n_links; i++) {
    snprintf(label, 80, "links[%zd]", i);
    dump_RpbLink(indent + 2, label, rcont->links[i], len);
  }
  printf("%*s  last_mod: %lu\n", indent, "", (unsigned long)rcont->last_mod);
  printf("%*s  last_mod_usecs: %lu\n", indent, "",
      (unsigned long)rcont->last_mod_usecs);
  printf("%*s  usermeta len: %zd\n", indent, "", rcont->n_usermeta);
  for (i = 0; i < rcont->n_usermeta; i++) {
    snprintf(label, 80, "usermeta[%zd]", i);
    dump_RpbPair(indent + 2, "usermeta", rcont->usermeta[i], len);
  }
  printf("%*s  indexes len: %zd\n", indent, "", rcont->n_indexes);
  for (i = 0; i < rcont->n_indexes; i++) {
    snprintf(label, 80, "indexes[%zd]", i);
    dump_RpbPair(indent + 2, "indexes", rcont->indexes[i], len);
  }
  printf("%*s  deleted: %d\n", indent, "", rcont->deleted);
  printf("\n");
}
