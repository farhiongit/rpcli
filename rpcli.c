#include "rpgrammar.tab.h"
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

#define _(str) (str) // i18n

static void
RPN_print_integer (integer a, void *) { printf ("= %lli [%s]\n", a, _ ("integer")); }

static void
RPN_print_decimal (decimal a, void *) { printf ("= %Lg\n", a); }

static void
RPN_print_error (const char *msg, void *) { fprintf (stderr, "%s: %s.\n", _ ("ERROR"), msg); }

static void
RPN_print_message (const char *format, va_list ap, void *) { vfprintf (stderr, format, ap); }

int
main (void) {
  setlocale (LC_ALL, "");

#ifdef RPN_DEBUG
#if RPN_DEBUG == 1
  RPN_debug = RPN_DEBUG;
#endif
#endif

  context *env = RPN_init ((struct report){ .integer = RPN_print_integer, .decimal = RPN_print_decimal, .error = RPN_print_error, .message = RPN_print_message });
  while (RPN_parse (env, stdin)) // On error, restart.
    ;
  RPN_cleanup (env);
}
