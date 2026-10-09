#include "rpgrammar.tab.h"
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wctype.h>

//=============== LEXER ====================
typedef jmp_buf exception;
#define throw_exception(exception) longjmp (exception, (__COUNTER__ + 1))
#define catch_exception(exception) if (setjmp (exception))

#define UNDEFINED_VALUE ((number){ .type = UNDEFINED })

static exception conversion_error;
static exception div_by_zero_error;

static number
from_integer (integer i) {
  return (number){ .type = INTEGER, .value.integer = i };
}

static number
from_decimal (decimal d) {
  return (number){ .type = DECIMAL, .value.decimal = d };
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
  errno = EPERM;
  RPN_error (0, "not a decimal");
  throw_exception (conversion_error);
  return NAN;
}

static integer
to_integer (number a) {
  switch (a.type) {
  case INTEGER:
    return a.value.integer;
    break;
  case DECIMAL:
  case UNDEFINED:
  default:
    break;
  }

  /* error handling : type mismatch */
  errno = EPERM;
  RPN_error (0, "not an integer");
  throw_exception (conversion_error);
  return 0;
}

// static symrec *sym_table = 0;

static symbol *
symrec_get (symrec *sym_table, const wchar_t *name) {
  for (symrec *s = sym_table; s; s = s->next)
    if (!wcscmp (name, s->symbol.name))
      return &s->symbol;

  return 0;
}

static symbol *
symrec_add (symrec **sym_table, symbol element) {
  symrec *symref = malloc (sizeof (*symref));
  if (!symref || !(element.name = wcsdup (element.name))) {
    errno = ENOMEM;
    RPN_error (0, "memory allocation error");
    return 0;
  } else {
    symref->symbol = element;
    symref->next = *sym_table;
    *sym_table = symref;
    return &symref->symbol;
  }
}

static symbol *
symrec_add_var (symrec **sym_table, wchar_t *name) {
  symbol *s;
  if ((s = symrec_get (*sym_table, name)))
    return s->type == VARIABLE ? s : 0;
  else
    return symrec_add (sym_table, (symbol){ .name = name,
                                            .type = VARIABLE,
                                            .value.number = UNDEFINED_VALUE });
}

int
RPN_lex (RPN_STYPE *RPN_lval, context *env) {
  wint_t c;
  while (iswblank (c = fgetwc (stdin))) // a space or a tab
    continue;

  if (iswspace (c)) // but not blank -> '\n', '\r', '\v', '\f'
    return RPN_EOL;
  if (c == WEOF)
    return RPN_END;

  ungetwc (c, stdin);
  wchar_t *wcs = malloc (sizeof (*wcs));
  *wcs = L'\0';
  size_t length = 0;
  while (!iswspace (c = fgetwc (stdin)) && c != WEOF) // any character can be part of a word
  {
    length++;
    wcs = realloc (wcs, (length + 1) * sizeof (*wcs));
    wcs[length - 1] = (wchar_t)c;
    wcs[length] = L'\0';
  }
  ungetwc (c, stdin);

  int token = -1;

  // last x
  if (token < 0 && !wcscmp (L"_", wcs)) {
    RPN_lval->number = env->last_x;
    token = RPN_NUMBER;
  }

  // last result
  if (token < 0 && !wcscmp (L"res", wcs)) {
    RPN_lval->number = env->last_res;
    token = RPN_NUMBER;
  }

  struct {
    const wchar_t *word;
    int token;
  } words[] = {
    // operators ('+', "-", ...)
    { L"+", '+' },
    { L"-", '-' },
    { L"/", '/' },
    { L"*", '*' },
    { L"//", RPN_QUOTIENT },
    { L"%", '%' },
    { L"^", '^' },
    { L"&", '&' },
    { L"|", '|' },
    { L"~", '~' },
    { L"**", RPN_POW },
    //
    { L"=", '=' },
    { L"vars", RPN_MEMORY },
    { L"exit", RPN_END },
  };

  // keywords
  for (size_t i = 0; token < 0 && i < sizeof (words) / sizeof (*words); i++)
    if (!wcscmp (words[i].word, wcs))
      token = words[i].token;

  // integer number
  if (token < 0) {
    errno = 0;
    wchar_t *endptr;
    RPN_lval->number = from_integer (wcstoll (wcs, &endptr, 0));
    if (!errno && !*endptr && endptr != wcs) {
      token = RPN_NUMBER;
    }
  }

  // decimal number
  if (token < 0) {
    errno = 0;
    wchar_t *endptr;
    RPN_lval->number = from_decimal (wcstold (wcs, &endptr));
    if (!errno && !*endptr && endptr != wcs) {
      token = RPN_NUMBER;
    }
  }

  // functions, constants and variables
  if (token < 0) {
    if (*wcs == L'_' || iswalpha ((wint_t)(*wcs))) {
      if ((RPN_lval->symbol = symrec_get (env->sym_table, wcs)))
        switch (RPN_lval->symbol->type) {
        case FUNCTION:
          switch (RPN_lval->symbol->value.function.type) {
          case IF:
          case DF:
            token = RPN_F0A;
            break;
          case IFI:
          case IFD:
          case DFI:
          case DFD:
            token = RPN_F1A;
            break;
          case IFII:
          case IFID:
          case IFDI:
          case IFDD:
          case DFII:
          case DFID:
          case DFDI:
          case DFDD:
            token = RPN_F2A;
            break;
          default:
            break;
          }
          break;
        case VARIABLE:
          token = RPN_VARIABLE;
          break;
        case CONSTANT:
          token = RPN_CONSTANT;
          break;
        default:
          break;
        }
      else {
        RPN_lval->symbol = symrec_add_var (&env->sym_table, wcs);
        token = RPN_NEW_VARIABLE;
      }
    }
  }

  free (wcs);

  return token < 0 ? RPN_UNRECOGNIZED : token;
}

//=============== ACTIONS FOR THE PARSER ====================
#define INTEGER_MAX LLONG_MAX
#define INTEGER_MIN LLONG_MIN
#define _(str) (str) // i18n

void
RPN_error (context *env, char const *msg) {
  (void)env;
  fprintf (stderr, "ERROR: %s.\n", _ (msg));
}

void
RPN_goodbye (symrec **sym_table) {
  while (*sym_table) {
    symrec *symref = *sym_table;
    *sym_table = (*sym_table)->next;
    free (symref->symbol.name);
    free (symref);
  }
  fprintf (stderr, "Good bye!\n");
}

static decimal
_rpn_sqr (decimal x) { return x * x; }

static decimal
_rpn_inverse (decimal x) { return (decimal)1 / x; }

static decimal
_rpn_opposite (decimal x) { return -x; }

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

void
RPN_print_number (number a) {
  switch (a.type) {
  case INTEGER:
    fprintf (stderr, "= ");
    printf ("%lli", a.value.integer);
    fflush (stdout);
    fprintf (stderr, " [integer]");
    break;
  case DECIMAL:
    fprintf (stderr, "= ");
    printf ("%Lg", a.value.decimal);
    fflush (stdout);
    break;
  case UNDEFINED:
    // return printf ("= ?");
    break;
  default:
    abort ();
  }
}

void
RPN_print_vars (symrec *sym_table) {
  for (symrec *s = sym_table; s; s = s->next)
    switch (s->symbol.type) {
    case CONSTANT:
    case VARIABLE:
      fprintf (stderr, "%ls ", s->symbol.name);
      RPN_print_number (s->symbol.value.number);
      fprintf (stderr, "\n");
    default:
    }
}

number
RPN_add (number a, number b) {
  catch_exception (conversion_error) {
    errno = EPERM;
    return UNDEFINED_VALUE;
  }

  if (a.type == INTEGER && b.type == INTEGER)
    return from_integer (to_integer (a) + to_integer (b));

  return from_decimal (to_decimal (a) + to_decimal (b));
}

number
RPN_sub (number a, number b) {
  catch_exception (conversion_error) {
    errno = EPERM;
    return UNDEFINED_VALUE;
  }

  if (a.type == INTEGER && b.type == INTEGER)
    return from_integer (to_integer (a) - to_integer (b));

  return from_decimal (to_decimal (a) - to_decimal (b));
}

number
RPN_mul (number a, number b) {
  catch_exception (conversion_error) {
    errno = EPERM;
    return UNDEFINED_VALUE;
  }

  if (a.type == INTEGER && b.type == INTEGER)
    return from_integer (to_integer (a) * to_integer (b));

  return from_decimal (to_decimal (a) * to_decimal (b));
}

number
RPN_div (number a, number b) {
  catch_exception (conversion_error) {
    errno = EPERM;
    return UNDEFINED_VALUE;
  }

  if (a.type == INTEGER && b.type == INTEGER && to_integer (b) != 0 && to_integer (a) % to_integer (b) == 0)
    return from_integer (to_integer (a) / to_integer (b));

  return from_decimal (to_decimal (a) / to_decimal (b));
}

number
RPN_quotient (number a, number b) {
  catch_exception (conversion_error) {
    errno = EPERM;
    return UNDEFINED_VALUE;
  }

  catch_exception (div_by_zero_error) {
    errno = EPERM;
    RPN_error (0, "division by zero");
    return UNDEFINED_VALUE;
  }

  if (to_integer (b))
    return from_integer (to_integer (a) / to_integer (b));

  throw_exception (div_by_zero_error);
}

number
RPN_mod (number a, number b) {
  catch_exception (conversion_error) {
    errno = EPERM;
    return UNDEFINED_VALUE;
  }

  catch_exception (div_by_zero_error) {
    errno = EPERM;
    RPN_error (0, "division by zero");
    return UNDEFINED_VALUE;
  }

  if (to_integer (b))
    return from_integer (to_integer (a) % to_integer (b));

  throw_exception (div_by_zero_error);
}

number
RPN_pow (number a, number b) {
  catch_exception (conversion_error) {
    errno = EPERM;
    return UNDEFINED_VALUE;
  }

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

number
RPN_and (number a, number b) {
  catch_exception (conversion_error) {
    errno = EPERM;
    return UNDEFINED_VALUE;
  }

  return from_integer (to_integer (a) & to_integer (b));
}

number
RPN_or (number a, number b) {
  catch_exception (conversion_error) {
    errno = EPERM;
    return UNDEFINED_VALUE;
  }

  return from_integer (to_integer (a) | to_integer (b));
}

number
RPN_xor (number a, number b) {
  catch_exception (conversion_error) {
    errno = EPERM;
    return UNDEFINED_VALUE;
  }

  return from_integer (to_integer (a) ^ to_integer (b));
}

number
RPN_compl (number a) {
  catch_exception (conversion_error) {
    errno = EPERM;
    return UNDEFINED_VALUE;
  }

  return from_integer (~to_integer (a));
}

number
RPN_f0a (symbol *symref) {
  catch_exception (conversion_error) {
    errno = EPERM;
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
    default:
      errno = EINVAL;
      break;
    }
    if (!errno) {
      errno = saveerrno;
      return ret;
    }
  } else if (s.value.number.type != UNDEFINED)
    return s.value.number;
  else
    RPN_error (0, "unknown variable");

  /* error handling */
  throw_exception (conversion_error);
}

number
RPN_set_var (symbol *symref, number val) {
  if (symref->type == VARIABLE && val.type != UNDEFINED)
    symref->value.number = val;

  return symref->value.number;
}

number
RPN_f1a (symbol *symref, number a) {
  catch_exception (conversion_error) {
    errno = EPERM;
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
    default:
      errno = EINVAL;
      break;
    }
    if (!errno) {
      errno = saveerrno;
      return ret;
    }
  }

  /* error handling */
  throw_exception (conversion_error);
}

number
RPN_f2a (symbol *symref, number a, number b) {
  catch_exception (conversion_error) {
    errno = EPERM;
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
    default:
      errno = EINVAL;
      break;
    }
    if (!errno) {
      errno = saveerrno;
      return ret;
    }
  }

  /* error handling */
  throw_exception (conversion_error);
}
//=============== MAIN ====================
int
main (void) {
  setlocale (LC_ALL, "");

  symrec *sym_table = 0;

#undef X
#define X              \
  DO (L"e", expl (1)); \
  DO (L"pi", acos (-1));
#undef DO
#define DO(_name, _val) symrec_add (&sym_table, (symbol){ .name = _name, .type = CONSTANT, .value.number = (number){ .type = DECIMAL, .value.decimal = _val } })
  X;

#undef X
#define X                    \
  DO (L"sin", sinl);         \
  DO (L"cos", cosl);         \
  DO (L"tan", tanl);         \
  DO (L"asin", asinl);       \
  DO (L"acos", acosl);       \
  DO (L"atan", atanl);       \
  DO (L"sinh", sinhl);       \
  DO (L"cosh", coshl);       \
  DO (L"tanh", tanhl);       \
  DO (L"asinh", asinhl);     \
  DO (L"acosh", acoshl);     \
  DO (L"atanh", atanhl);     \
  DO (L"exp", expl);         \
  DO (L"ln", logl);          \
  DO (L"log", log10l);       \
  DO (L"sqrt", sqrtl);       \
  DO (L"abs", fabsl);        \
  DO (L"sqr", _rpn_sqr);     \
  DO (L"inv", _rpn_inverse); \
  DO (L"neg", _rpn_opposite);
#undef DO
#define DO(_name, _val) symrec_add (&sym_table, (symbol){ .name = _name, .type = FUNCTION, .value.function = (function){ .type = DFD, .value.dFd = _val } })
  X;

#undef X
#define X \
  DO (L"rand", _rpn_random);
#undef DO
#define DO(_name, _val) symrec_add (&sym_table, (symbol){ .name = _name, .type = FUNCTION, .value.function = (function){ .type = DF, .value.dF = _val } })
  X;

#undef X
#define X                  \
  DO (L"round", llroundl); \
  DO (L"ceil", _rpn_ceil); \
  DO (L"floor", _rpn_floor);
#undef DO
#define DO(_name, _val) symrec_add (&sym_table, (symbol){ .name = _name, .type = FUNCTION, .value.function = (function){ .type = IFD, .value.iFd = _val } })
  X;

#ifdef RPN_DEBUG
#if RPN_DEBUG == 1
  RPN_debug = RPN_DEBUG;
#endif
#endif

  fprintf (stderr, _ ("Type Ctrl-d or '%s' to quit.\n"), _ ("exit"));

  context env = { .sym_table = sym_table };
  while (RPN_parse (&env)) // On error, restart.
    ;
}
