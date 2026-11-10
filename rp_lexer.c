#undef NDEBUG
#include "genericstreamreader.h"
#include "rp_types.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

#define _(str) (str) // i18n

//=============== LEXER ====================
static symbol *
from_name (context *env, const wchar_t *name) {
  for (symrec *s = env->sym_table; s; s = s->next)
    if (!wcscmp (name, s->symbol.name))
      return &s->symbol;

  return 0;
}

static int
RPN_lex (RPN_STYPE *RPN_lval, context *env, StreamReader *in) {
  wchar_t *c;
  while ((c = stream_look_ahead (in, 1)) && iswblank ((wint_t)*c)) // a space or a tab
    stream_get_and_forget (in);

  if (!(c = stream_look_ahead (in, 1))) // End of stream
    return RPN_END;
  else if (iswspace ((wint_t)*c)) { // but not blank -> '\n', '\r', '\v', '\f'
    stream_get_and_forget (in);
    return RPN_EOL;
  } else if (*c == L';') {
    stream_get_and_forget (in);
    return RPN_SEP;
  }

  wchar_t *wcs = malloc (sizeof (*wcs));
  wcs[0] = L'\0';
  size_t length = 0;
  while ((c = stream_look_ahead (in, length + 1)) && !iswspace ((wint_t)*c)) { // any character can be part of a word (even not printable)
    wcs[length] = (wchar_t)(*c);
    wcs = realloc (wcs, (++length + 1) * sizeof (*wcs));
    wcs[length] = L'\0';
  }

  int token = -1;
  size_t advance = 0;

  // number
  if (token < 0) {
    errno = 0;
    wchar_t *endld;
    number ld = from_decimal (wcstold (wcs, &endld));
    advance = (size_t)(endld - wcs);
    if (errno == ERANGE)
      _rpn_report_error (env, _ ("out of range"));
    else if (!errno && endld != wcs) {
      token = RPN_NUMBER;
      RPN_lval->number = ld;
    }

    errno = 0;
    wchar_t *endll;
    number ll = from_integer (wcstoll (wcs, &endll, 0));
    if (endll >= endld) {
      advance = (size_t)(endll - wcs);
      token = -1;
      if (errno == ERANGE)
        _rpn_report_error (env, _ ("out of range"));
      else if (!errno && endll != wcs) {
        token = RPN_NUMBER;
        RPN_lval->number = ll;
      }
    }
  }

  wchar_t *validkw = wcsdup (wcs);
  wchar_t *wc = validkw;
  for (; (iswpunct ((wint_t)*validkw) ? iswpunct : iswalpha) ((wint_t)*wc); wc++) {
    /* valid keywords are either only letters or only punctuation */
  }
  *wc = L'\0';

  static struct {
    const wchar_t *word;
    int token;
  } words[] = {
    { L"=", RPN_ASSIGN },
    { L"+", RPN_ADD },
    { L"*", RPN_MUL },
    { L"_", RPN_LAST_X },
    { L"res", RPN_LAST_RESULT },
    { L"ans", RPN_LAST_ANSWER },
    { L"vars", RPN_MEMORY },
    { L"exit", RPN_END },
  };

  // keywords
  for (size_t i = 0; token < 0 && *validkw != L'\0' && i < sizeof (words) / sizeof (*words); i++)
    if (!wcscmp (words[i].word, validkw)) {
      token = words[i].token;
      advance = wcslen (words[i].word);
      free (validkw);
    }

  // functions, constants and variables
  if (token < 0 && *validkw != L'\0') {
    if ((RPN_lval->symbol = from_name (env, validkw))) {
      advance = wcslen (RPN_lval->symbol->name);
      switch (RPN_lval->symbol->type) {
      case FUNCTION:
        switch (RPN_lval->symbol->value.function.type) {
        case IF:
        case DF:
        case NF:
          token = RPN_F0A;
          break;
        case IFI:
        case IFD:
        case DFI:
        case DFD:
        case NFN:
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
        case NFNN:
          token = RPN_F2A;
          break;
        default:
          break;
        }
        break;
      case VARIABLE:
        token = RPN_KNOWN_VARIABLE;
        break;
      case CONSTANT:
        token = RPN_CONSTANT;
        break;
      default:
        break;
      }
      free (validkw);
    } // if ((RPN_lval->symbol = from_name (env, validkw)))
    else if (iswalpha ((wint_t)*validkw)) {
      token = RPN_NEW_VARIABLE;
      RPN_lval->newvar_name = validkw;
      advance = wcslen (validkw);
    }
  } // if (token < 0 && *validkw != L'\0')

  if (token < 0)
    advance = wcslen (wcs); // Discard the whole word.

  for (; advance; advance--)
    stream_get_and_forget (in);

  free (wcs);

  return token < 0 ? RPN_UNRECOGNIZED : token;
}
