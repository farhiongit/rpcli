#ifndef __RP_PARSER_H__
#define __RP_PARSER_H__

#include "rp_types.h"

struct report {
  void (*error) (const char *, void *);
  void (*decimal) (decimal, void *);
  void (*integer) (integer, void *);
  void (*message) (const char *format, va_list, void *);
  void *arg;
};
context *RPN_init (struct report r);
void RPN_cleanup (context *env);

int RPN_lex (RPN_STYPE *RPN_lval, context *env, FILE *in);
symbol *from_name (context *env, const wchar_t *name);
number from_integer (integer);
number from_decimal (decimal);

#endif
