#include "genericstreamreader.h"
#include "rp_types.h"
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
RPN_lex (RPN_STYPE *RPN_lval, context *env, StreamReader *in /* reading wchar_t */) {
  wchar_t *c;
  while ((c = stream_look_ahead (in, 1)) && iswblank ((wint_t)*c)) // a space or a tab
    stream_get_and_forget (in);

  if (!(c = stream_look_ahead (in, 1))) // End of stream
    return RPN_END;
  else if (iswspace ((wint_t)*c)) { // but not iswblank -> '\n', '\r', '\v', '\f'
    stream_get_and_forget (in);
    return RPN_EOL;
  } else if (*c == L';') {
    stream_get_and_forget (in);
    return RPN_SEP;
  }

  wchar_t *wcs = malloc (sizeof (*wcs));
  if (!wcs) {
    _rpn_report_error (env, _ ("memory exhausted"));
    return RPN_UNRECOGNIZED;
  }
  wcs[0] = L'\0';
  size_t length = 0;
  while ((c = stream_look_ahead (in, length + 1)) && !iswspace ((wint_t)*c)) { // any character can be part of a word (even not printable)
    wcs[length] = (wchar_t)(*c);
    wcs = realloc (wcs, (++length + 1) * sizeof (*wcs));
    if (!wcs) {
      _rpn_report_error (env, _ ("memory exhausted"));
      return RPN_UNRECOGNIZED;
    }
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

  // keyword or variable
  if (token < 0) {
    wchar_t *validkw = wcsdup (wcs);
    if (!validkw)
      _rpn_report_error (env, _ ("memory exhausted"));
    else {
      wchar_t *wc = validkw;
      // a valid keywords or variables are either only letters or only punctuation
      for (; (iswpunct ((wint_t)*validkw) ? iswpunct : iswalpha) ((wint_t)*wc); wc++) {
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
          advance = wcslen (validkw);
        }

      // functions, constants and variables
      if (token < 0 && *validkw != L'\0') {
        if ((RPN_lval->symbol = from_name (env, validkw))) {
          advance = wcslen (validkw);
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
        } // if ((RPN_lval->symbol = from_name (env, validkw)))
        else if (iswalpha ((wint_t)*validkw)) {
          token = RPN_NEW_VARIABLE;
          advance = wcslen (validkw);
          // "Transfer ownership" to the parser (see the grammar: %destructor { free ($$); } <newvar_name>)
          RPN_lval->newvar_name = validkw;
          validkw = 0; // won't be free'd by the lexer.
        }
      } // if (token < 0 && *validkw != L'\0')

      if (validkw)
        free (validkw);
    } //  if (validkw)
  } // if (token < 0)

  if (token < 0)
    advance = wcslen (wcs); // Discard the whole word.

  for (; advance; advance--)
    stream_get_and_forget (in);

  free (wcs);

  return token < 0 ? RPN_UNRECOGNIZED : token;
}
