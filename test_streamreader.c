#undef NDEBUG
#include "genericstreamreader.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define USED(expr) \
  do {             \
    if ((expr)) {  \
    }              \
  } while (0)

#define LIST_OF_TOKENS                                   \
  ON_TOKEN_DO (EOF)                                      \
  ON_TOKEN_DO (ASSIGN)      /* = */                      \
  ON_TOKEN_DO (SEMICOLON)   /* ; */                      \
  ON_TOKEN_DO (INT_LITERAL) /* Example : 42 */           \
  ON_TOKEN_DO (IDENTIFIER)  /* Example : variable, fn */ \
  ON_TOKEN_DO (KEYWORD)     /* Example : return, if */   \
  ON_TOKEN_DO (UNKNOWN)

#undef ON_TOKEN_DO
#define ON_TOKEN_DO(token) TOKEN_##token,
typedef enum {
  LIST_OF_TOKENS
} TokenType;

typedef struct {
  TokenType type;
  char *value; // Chaîne de caractères allouée dynamiquement pour la valeur du token
} Token;

void token_free (Token token);
Token next_token (StreamReader *stream);

// Libération de la mémoire d'un token
void
token_free (Token token) {
  if (token.value) {
    free (token.value);
  }
}

static char *keywords[] = {
  "return",
  "if",
  "fn",
};

// Fonction utilitaire pour vérifier si une chaîne est un mot-clé
static bool
is_keyword (const char *str) {
  for (size_t i = 0; i < sizeof (keywords) / sizeof (*keywords); i++)
    if (strcmp (str, keywords[i]) == 0)
      return 1;
  return 0;
}

// Fonction principale du Lexer : extrait le prochain Token du flux générique
Token
next_token (StreamReader *stream) {
  char *c = stream_look_ahead (stream, 1);

  // 1. Ignorer les espaces blancs
  while (c && (*c == ' ' || *c == '\t' || *c == '\n' || *c == '\r')) {
    USED (stream_get (stream));
    c = stream_look_ahead (stream, 1);
  }

  // 2. Gestion de la fin de fichier
  if (!c) {
    return (Token){ .type = TOKEN_EOF, .value = strdup ("EOF") };
  }

  // 3. Identification des NOMBRES (Ex: 42)
  if (c && isdigit (*c)) {
    size_t len = 0;
    // Utilisation du look-ahead pour mesurer la taille du nombre
    while ((c = stream_look_ahead (stream, len + 1)) && isdigit (*c)) {
      len++;
    }

    // Allocation et extraction
    char *buffer = malloc ((len + 1) * sizeof (*buffer));
    for (size_t i = 0; i < len; i++) {
      buffer[i] = *(char *)stream_get (stream);
    }
    buffer[len] = '\0';

    return (Token){ .type = TOKEN_INT_LITERAL, .value = buffer };
  }

  // 4. Identification des IDENTIFIANTS et MOTS-CLÉS
  if (c && (isalpha (*c) || *c == '_')) {
    size_t len = 0;
    char *ahead = stream_look_ahead (stream, len + 1);
    while (ahead && (isalnum (*ahead) || *ahead == '_')) {
      len++;
      ahead = stream_look_ahead (stream, len + 1);
    }

    char *buffer = malloc ((len + 1) * sizeof (*buffer));
    for (size_t i = 0; i < len; i++) {
      buffer[i] = *(char *)stream_get (stream);
    }
    buffer[len] = '\0';

    // Distinction entre mot-clé et identifiant générique
    TokenType type = is_keyword (buffer) ? TOKEN_KEYWORD : TOKEN_IDENTIFIER;
    return (Token){ .type = type, .value = buffer };
  }

  // 5. Identification des symboles simples (Opérateurs / Ponctuation)
  if (c && *c == '=') {
    USED (stream_get (stream));
    return (Token){ .type = TOKEN_ASSIGN, .value = strdup ("=") };
  }
  if (c && *c == ';') {
    USED (stream_get (stream));
    return (Token){ .type = TOKEN_SEMICOLON, .value = strdup (";") };
  }

  // 6. Caractère inconnu
  USED (stream_get (stream));
  char unknown_str[2] = { *(char *)c, '\0' };
  return (Token){ .type = TOKEN_UNKNOWN, .value = strdup (unknown_str) };
}

// ========================== test
#undef ON_TOKEN_DO
#define ON_TOKEN_DO(token) \
  case TOKEN_##token:      \
    return #token;
static const char *
token_type_to_string (TokenType type) {
  switch (type) {
    LIST_OF_TOKENS
  default:
    return "UNKNOWN";
  }
}

int
main (void) {
  // Code source à analyser
  char *source_code = "return 42; score = 100;";

  StreamReader *stream = stream_create (str_reader (source_code));

  printf ("\"%s\" =\n\n", source_code);

  Token tok;
  do {
    tok = next_token (stream);
    printf ("[%s] -> \"%s\"\n", token_type_to_string (tok.type), tok.value);
    token_free (tok); // Libère la chaîne du token courant
  } while (tok.type != TOKEN_EOF);

  stream_destroy (stream);
  return EXIT_SUCCESS;
}
