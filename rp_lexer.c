#include "rpgrammar.tab.h"
#include <errno.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>

#define _(str) (str) // i18n

//=============== LEXER ====================
int
RPN_lex (RPN_STYPE *RPN_lval, context *env, FILE *in) {
  wint_t c;
  while (iswblank (c = fgetwc (in))) // a space or a tab
    continue;

  if (iswspace (c)) // but not blank -> '\n', '\r', '\v', '\f'
    return RPN_EOL;
  if (c == WEOF)
    return RPN_END;

  ungetwc (c, in);
  wchar_t *wcs = malloc (sizeof (*wcs));
  *wcs = L'\0';
  size_t length = 0;
  while (!iswspace (c = fgetwc (in)) && c != WEOF) // any character can be part of a word
  {
    length++;
    wcs = realloc (wcs, (length + 1) * sizeof (*wcs));
    wcs[length - 1] = (wchar_t)c;
    wcs[length] = L'\0';
  }
  ungetwc (c, in);

  int token = -1;

  static struct {
    const wchar_t *word;
    int token;
  } words[] = {
    { L"=", '=' },
    { L"_", RPN_LAST_X },
    { L"res", RPN_LAST_RESULT },
    { L"ans", RPN_LAST_ANSWER },
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
    if ((RPN_lval->symbol = from_name (env, wcs))) {
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
        if (*wcs == L'_' || iswalpha ((wint_t)(*wcs)))
          token = RPN_KNOWN_VARIABLE;
        break;
      case CONSTANT:
        token = RPN_CONSTANT;
        break;
      default:
        break;
      }
    } else if (*wcs == L'_' || iswalpha ((wint_t)(*wcs))) {
      RPN_lval->newvar_name = wcsdup (wcs);
      token = RPN_NEW_VARIABLE;
    }
  }

  free (wcs);

  return token < 0 ? RPN_UNRECOGNIZED : token;
}
