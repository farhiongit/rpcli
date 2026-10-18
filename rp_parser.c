#include "rp_parser.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdckdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define _(str) (str) // i18n

typedef jmp_buf exception;
#define throw_exception(exception) longjmp (exception, (__COUNTER__ + 1))
#define catch_exception(exception) if (setjmp (exception))

//=============== FOR THE PARSER ====================
#define UNDEFINED_VALUE ((struct number){ .type = UNDEFINED })
#define INTEGER_MAX LLONG_MAX
#define INTEGER_MIN LLONG_MIN

static exception conversion_error;
static exception div_by_zero_error;
static exception out_of_range_error;
static exception unknown_variable_error;

typedef struct symrec symrec;
struct symrec {
  symbol symbol;
  struct symrec *next;
};

struct context {
  symrec *sym_table;
  number last_x, last_res, last_ans;
  struct report report;
};

static void
RPN_error (context *env, FILE *in, char const *msg) {
  (void)in;
  if (env && env->report.error)
    env->report.error (msg, env->report.arg);
  else
    fprintf (stderr, "%s: %s.\n", _ ("ERROR"), msg);
}

static void
_rpn_report_error (context *env, char const *msg) {
  RPN_error (env, 0, msg);
}

static void
_rpn_printer_number (number n) {
  fflush (stdout);
  switch (n.type) {
  case INTEGER:
    fprintf (stderr, "%lli", n.value.integer);
    break;
  case DECIMAL:
    fprintf (stderr, "%Lg", n.value.decimal);
    break;
  case UNDEFINED:
    fprintf (stderr, "?");
    break;
  }
}

static void
_rpn_report_number (context *env, number n) {
  switch (n.type) {
  case INTEGER:
    if (env->report.integer)
      env->report.integer (n.value.integer, env->report.arg);
    else
      fprintf (stderr, "%lli\n", n.value.integer);
    break;
  case DECIMAL:
    if (env->report.decimal)
      env->report.decimal (n.value.decimal, env->report.arg);
    else
      fprintf (stderr, "%Lg\n", n.value.decimal);
    break;
  case UNDEFINED:
    break;
  }
}

static void
_rpn_report_message (context *env, const char *restrict format, ...) {
  if (env->report.message) {
    va_list ap;
    va_start (ap, format);
    env->report.message (format, ap, env->report.arg);
    va_end (ap);
  }
}

static symbol *
symrec_add (context *env, symbol element) {
  symrec *symref = malloc (sizeof (*symref));
  if (!symref || !(element.name = wcsdup (element.name))) {
    errno = ENOMEM;
    _rpn_report_error (env, _ ("memory allocation error"));
    return 0;
  } else {
    symref->symbol = element;
    symref->next = env->sym_table;
    env->sym_table = symref;
    return &symref->symbol;
  }
}

static symbol *
RPN_add_var (context *env, wchar_t *name) {
  return symrec_add (env, (symbol){ .name = name,
                                    .type = VARIABLE,
                                    .value.number = UNDEFINED_VALUE });
}

static number
RPN_set_var (symbol *symref, number val) {
  if (symref->type == VARIABLE && val.type != UNDEFINED)
    symref->value.number = val;

  return symref->value.number;
}

static void
RPN_print_vars (context *env) {
  for (symrec *s = env->sym_table; s; s = s->next)
    if (s->symbol.type == VARIABLE) {
      _rpn_report_message (env, "%ls ", s->symbol.name);
      _rpn_report_number (env, s->symbol.value.number);
    }
}

static decimal
to_decimal (number a) {
  switch (a.type) {
  case INTEGER:
    return a.value.integer;
    break;
  case DECIMAL:
    return a.value.decimal;
    break;
  case UNDEFINED:
  default:
    break;
  }

  /* error handling : type mismatch */
  throw_exception (conversion_error);

  errno = EPERM;
  return NAN;
}

static integer
to_integer (number a) {
  switch (a.type) {
  case INTEGER:
    return a.value.integer;
    break;
  case DECIMAL:
    if (iszero (a.value.decimal - llroundl (a.value.decimal)))
      return llroundl (a.value.decimal);
    break;
  case UNDEFINED:
  default:
    break;
  }

  /* error handling : type mismatch */
  throw_exception (conversion_error);

  errno = EPERM;
  return 0;
}

static number
_rpn_add (number a, number b) {
  if (a.type == INTEGER && b.type == INTEGER) {
    integer result;
    if (!ckd_add (&result, to_integer (a), to_integer (b)))
      return from_integer (result);
    else
      throw_exception (out_of_range_error);
  }

  return from_decimal (to_decimal (a) + to_decimal (b));
}

static number
RPN_add (context *env, number a, number b) {
  catch_exception (conversion_error) {
    errno = EPERM;
    _rpn_report_error (env, _ ("type mismatch"));
    return UNDEFINED_VALUE;
  }

  catch_exception (out_of_range_error) {
    errno = EPERM;
    _rpn_report_error (env, _ ("out of range"));
    return UNDEFINED_VALUE;
  }

  return _rpn_add (a, b);
}

static number
_rpn_mul (number a, number b) {
  if (a.type == INTEGER && b.type == INTEGER) {
    integer result;
    if (!ckd_mul (&result, to_integer (a), to_integer (b)))
      return from_integer (result);
    else
      throw_exception (out_of_range_error);
  }

  return from_decimal (to_decimal (a) * to_decimal (b));
}

static number
RPN_mul (context *env, number a, number b) {
  catch_exception (conversion_error) {
    errno = EPERM;
    _rpn_report_error (env, _ ("type mismatch"));
    return UNDEFINED_VALUE;
  }

  catch_exception (out_of_range_error) {
    errno = EPERM;
    _rpn_report_error (env, _ ("out of range"));
    return UNDEFINED_VALUE;
  }

  return _rpn_mul (a, b);
}

static number
_rpn_f0a (context *env, symbol *symref) {
  catch_exception (conversion_error) {
    errno = EPERM;
    _rpn_report_error (env, _ ("type mismatch"));
    return UNDEFINED_VALUE;
  }

  catch_exception (out_of_range_error) {
    errno = EPERM;
    _rpn_report_error (env, _ ("out of range"));
    return UNDEFINED_VALUE;
  }

  catch_exception (div_by_zero_error) {
    errno = EPERM;
    _rpn_report_error (env, _ ("division by zero"));
    return UNDEFINED_VALUE;
  }

  catch_exception (unknown_variable_error) {
    errno = EPERM;
    _rpn_report_error (env, _ ("unknown variable"));
    return UNDEFINED_VALUE;
  }

  symbol s = *symref;
  if (s.type == FUNCTION) {
    int saveerrno = errno;
    errno = 0;
    number ret = UNDEFINED_VALUE;
    switch (s.value.function.type) {
    case IF:
      ret = from_integer (s.value.function.value.iF ());
      break;
    case DF:
      ret = from_decimal (s.value.function.value.dF ());
      break;
    case NF:
      ret = s.value.function.value.nF ();
      break;
    default:
      errno = EINVAL;
      break;
    }
    if (!errno) {
      errno = saveerrno;
      return ret;
    } else
      throw_exception (out_of_range_error);
  } else if (s.value.number.type != UNDEFINED)
    return s.value.number;
  else
    throw_exception (unknown_variable_error);

  /* error handling */
  throw_exception (conversion_error);
}

static number
_rpn_f1a (context *env, symbol *symref, number a) {
  catch_exception (conversion_error) {
    errno = EPERM;
    _rpn_report_error (env, _ ("type mismatch"));
    return UNDEFINED_VALUE;
  }

  catch_exception (out_of_range_error) {
    errno = EPERM;
    _rpn_report_error (env, _ ("out of range"));
    return UNDEFINED_VALUE;
  }

  catch_exception (div_by_zero_error) {
    errno = EPERM;
    _rpn_report_error (env, _ ("division by zero"));
    return UNDEFINED_VALUE;
  }

  symbol s;
  if (symref && (s = *symref).type == FUNCTION) {
    int saveerrno = errno;
    errno = 0;
    number ret = UNDEFINED_VALUE;
    switch (s.value.function.type) {
    case IFI:
      ret = from_integer (s.value.function.value.iFi (to_integer (a)));
      break;
    case IFD:
      ret = from_integer (s.value.function.value.iFd (to_decimal (a)));
      break;
    case DFI:
      ret = from_decimal (s.value.function.value.dFi (to_integer (a)));
      break;
    case DFD:
      ret = from_decimal (s.value.function.value.dFd (to_decimal (a)));
      break;
    case NFN:
      ret = s.value.function.value.nFn (a);
      break;
    default:
      errno = EINVAL;
      break;
    }
    if (!errno) {
      errno = saveerrno;
      return ret;
    } else
      throw_exception (out_of_range_error);
  }

  /* error handling */
  throw_exception (conversion_error);
}

static number
_rpn_f2a (context *env, symbol *symref, number a, number b) {
  catch_exception (conversion_error) {
    errno = EPERM;
    _rpn_report_error (env, _ ("type mismatch"));
    return UNDEFINED_VALUE;
  }

  catch_exception (out_of_range_error) {
    errno = EPERM;
    _rpn_report_error (env, _ ("out of range"));
    return UNDEFINED_VALUE;
  }

  catch_exception (div_by_zero_error) {
    errno = EPERM;
    _rpn_report_error (env, _ ("division by zero"));
    return UNDEFINED_VALUE;
  }

  symbol s;
  if (symref && (s = *symref).type == FUNCTION) {
    int saveerrno = errno;
    errno = 0;
    number ret = UNDEFINED_VALUE;
    switch (s.value.function.type) {
    case IFII:
      ret = from_integer (
          s.value.function.value.iFii (to_integer (a), to_integer (b)));
      break;
    case IFID:
      ret = from_integer (
          s.value.function.value.iFid (to_integer (a), to_decimal (b)));
      break;
    case IFDI:
      ret = from_integer (
          s.value.function.value.iFdi (to_decimal (a), to_integer (b)));
      break;
    case IFDD:
      ret = from_integer (
          s.value.function.value.iFdd (to_decimal (a), to_decimal (b)));
      break;
    case DFII:
      ret = from_decimal (
          s.value.function.value.iFii (to_integer (a), to_integer (b)));
      break;
    case DFID:
      ret = from_decimal (
          s.value.function.value.iFid (to_integer (a), to_decimal (b)));
      break;
    case DFDI:
      ret = from_decimal (
          s.value.function.value.iFdi (to_decimal (a), to_integer (b)));
      break;
    case DFDD:
      ret = from_decimal (
          s.value.function.value.iFdd (to_decimal (a), to_decimal (b)));
      break;
    case NFNN:
      ret = s.value.function.value.nFnn (a, b);
      break;
    default:
      errno = EINVAL;
      break;
    }
    if (!errno) {
      errno = saveerrno;
      return ret;
    } else
      throw_exception (out_of_range_error);
  }

  /* error handling */
  throw_exception (conversion_error);
}

static number
_rpn_sqr (number x) { return _rpn_mul (x, x); }

static decimal
_rpn_inverse (decimal x) { return (decimal)1 / x; }

static number
_rpn_opposite (number x) {
  switch (x.type) {
  case INTEGER:
    if ((-to_integer (x) > 0 && to_integer (x) > 0) || (-to_integer (x) < 0 && to_integer (x) < 0))
      throw_exception (out_of_range_error);
    return from_integer (-to_integer (x));
  case DECIMAL:
    return from_decimal (-to_decimal (x));
  default:
    errno = EPERM;
    return UNDEFINED_VALUE;
  }
}

static integer
_rpn_ceil (decimal x) { return llroundl (ceill (x)); }

static integer
_rpn_floor (decimal x) { return llroundl (floorl (x)); }

static decimal
_rpn_random (void) {
  static long int seed = 0;
  static struct drand48_data buf = { 0 };
  if (!seed)
    srand48_r (seed = time (0), &buf);

  double r;
  if (drand48_r (&buf, &r) == 0)
    return (decimal)r;

  return 0;
}

static number
_rpn_sub (number a, number b) {
  if (a.type == INTEGER && b.type == INTEGER) {
    integer result;
    if (!ckd_sub (&result, to_integer (a), to_integer (b)))
      return from_integer (result);
    else
      throw_exception (out_of_range_error);
  }

  return from_decimal (to_decimal (a) - to_decimal (b));
}

static number
_rpn_div (number a, number b) {
  if (a.type == INTEGER && b.type == INTEGER && to_integer (b) != 0 && to_integer (a) % to_integer (b) == 0)
    return from_integer (to_integer (a) / to_integer (b));

  return from_decimal (to_decimal (a) / to_decimal (b));
}

static integer
_rpn_quotient (integer a, integer b) {
  if (b)
    return a / b;

  throw_exception (div_by_zero_error);

  errno = EPERM;
  return 0;
}

static number
_rpn_mod (number a, number b) {
  if (a.type == INTEGER && to_integer (b) == 0)
    throw_exception (div_by_zero_error);

  if (a.type == INTEGER && b.type == INTEGER)
    return from_integer (to_integer (a) % to_integer (b));
  else
    return from_decimal (fmodl (to_decimal (a), to_decimal (b)));

  errno = EPERM;
  return UNDEFINED_VALUE;
}

static number
_rpn_pow (number a, number b) {
  int saveerrno = errno;
  errno = 0;
  number ret = UNDEFINED_VALUE;
  if (a.type == INTEGER && b.type == INTEGER && to_integer (b) >= 0) {
    integer pow = 1;
    for (integer i = to_integer (b); i > 0 && !errno; i--)
      if (pow * to_integer (a) <= INTEGER_MAX)
        pow *= to_integer (a);
      else
        errno = EOVERFLOW;
    ret = from_integer (pow);
    if (!errno) {
      errno = saveerrno;
      return ret;
    }
  }
  errno = 0;
  ret = from_decimal (powl (to_decimal (a), to_decimal (b)));
  if (!errno) {
    errno = saveerrno;
    return ret;
  }

  throw_exception (conversion_error);
}

static integer
_rpn_and (integer a, integer b) { return a & b; }

static integer
_rpn_or (integer a, integer b) { return a | b; }

static integer
_rpn_xor (integer a, integer b) { return a ^ b; }

static integer
_rpn_compl (integer a) { return ~a; }

static integer
_rpn_fact (integer a) {
  integer ret = 1;
  if (a < 0) {
    throw_exception (out_of_range_error);
    errno = EPERM;
    return 0;
  }

  for (; a > 0; a--)
    if (a * ret < 0)
      throw_exception (out_of_range_error);
    else
      ret *= a;
  return ret;
}

static number
_rpn_abs (number a) {
  switch (a.type) {
  case INTEGER:
    if (to_integer (a) < 0 && -to_integer (a) < 0)
      throw_exception (out_of_range_error);
    return to_integer (a) < 0 ? from_integer (-to_integer (a)) : a;
  case DECIMAL:
    return from_decimal (fabsl (to_decimal (a)));
  default:
    return UNDEFINED_VALUE;
  }

  errno = EPERM;
  return UNDEFINED_VALUE;
}

//=============== FOR THE PARSER INTERFACE ====================
static context *
_rpn_init (void) {
  context *env = calloc (1, sizeof (*env));
  if (!env) {
    errno = ENOMEM;
    _rpn_report_error (0, _ ("memory allocation error"));
    return 0;
  }

  env->sym_table = 0;

#undef X
#define X              \
  DO (L"e", expl (1)); \
  DO (L"pi", acos (-1));
#undef DO
#define DO(_name, _val) symrec_add (env, (symbol){ .name = (_name), .type = CONSTANT, .value.number = (number){ .type = DECIMAL, .value.decimal = (_val) } })
  X;

#undef X
#define X                \
  DO (L"sin", sinl);     \
  DO (L"cos", cosl);     \
  DO (L"tan", tanl);     \
  DO (L"asin", asinl);   \
  DO (L"acos", acosl);   \
  DO (L"atan", atanl);   \
  DO (L"sinh", sinhl);   \
  DO (L"cosh", coshl);   \
  DO (L"tanh", tanhl);   \
  DO (L"asinh", asinhl); \
  DO (L"acosh", acoshl); \
  DO (L"atanh", atanhl); \
  DO (L"exp", expl);     \
  DO (L"ln", logl);      \
  DO (L"log", log10l);   \
  DO (L"sqrt", sqrtl);   \
  DO (L"inv", _rpn_inverse);
#undef DO
#define DO(_name, _val) symrec_add (env, (symbol){ .name = (_name), .type = FUNCTION, .value.function = (function){ .type = DFD, .value.dFd = (_val) } })
  X;

#undef X
#define X \
  DO (L"rand", _rpn_random);
#undef DO
#define DO(_name, _val) symrec_add (env, (symbol){ .name = (_name), .type = FUNCTION, .value.function = (function){ .type = DF, .value.dF = (_val) } })
  X;

#undef X
#define X                  \
  DO (L"round", llroundl); \
  DO (L"ceil", _rpn_ceil); \
  DO (L"floor", _rpn_floor);
#undef DO
#define DO(_name, _val) symrec_add (env, (symbol){ .name = (_name), .type = FUNCTION, .value.function = (function){ .type = IFD, .value.iFd = (_val) } })
  X;

#undef X
#define X                \
  DO (L"~", _rpn_compl); \
  DO (L"!", _rpn_fact);
#undef DO
#define DO(_name, _val) symrec_add (env, (symbol){ .name = (_name), .type = FUNCTION, .value.function = (function){ .type = IFI, .value.iFi = (_val) } })
  X;

#undef X
#define X                \
  DO (L"abs", _rpn_abs); \
  DO (L"sqr", _rpn_sqr); \
  DO (L"neg", _rpn_opposite);
#undef DO
#define DO(_name, _val) symrec_add (env, (symbol){ .name = (_name), .type = FUNCTION, .value.function = (function){ .type = NFN, .value.nFn = (_val) } })
  X;

#undef X
#define X                    \
  DO (L"//", _rpn_quotient); \
  DO (L"|", _rpn_or);        \
  DO (L"&", _rpn_and);       \
  DO (L"^", _rpn_xor);
#undef DO
#define DO(_name, _val) symrec_add (env, (symbol){ .name = (_name), .type = FUNCTION, .value.function = (function){ .type = IFII, .value.iFii = (_val) } })
  X;

#undef X
#define X              \
  DO (L"+", _rpn_add); \
  DO (L"-", _rpn_sub); \
  DO (L"*", _rpn_mul); \
  DO (L"/", _rpn_div); \
  DO (L"%", _rpn_mod); \
  DO (L"pow", _rpn_pow);
#undef DO
#define DO(_name, _val) symrec_add (env, (symbol){ .name = (_name), .type = FUNCTION, .value.function = (function){ .type = NFNN, .value.nFnn = (_val) } })
  X;

  return env;
}

context *
RPN_init (struct report r) {
  context *env = _rpn_init ();
  if (env)
    env->report = r;
  return env;
}

void
RPN_cleanup (context *env) {
  while (env->sym_table) {
    symrec *symref = env->sym_table;
    env->sym_table = env->sym_table->next;
    free (symref->symbol.name);
    free (symref);
  }
  free (env);
}

symbol *
from_name (context *env, const wchar_t *name) {
  for (symrec *s = env->sym_table; s; s = s->next)
    if (!wcscmp (name, s->symbol.name))
      return &s->symbol;

  return 0;
}

number
from_integer (integer i) {
  return (number){ .type = INTEGER, .value.integer = i };
}

number
from_decimal (decimal d) {
  return (number){ .type = DECIMAL, .value.decimal = d };
}
