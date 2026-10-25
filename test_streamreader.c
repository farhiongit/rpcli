#undef NDEBUG
#include "str_reader_adapter.h"
#include "streamreader.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  TOKEN_EOF = 0,
  TOKEN_INT_LITERAL, // Exemple : 42
  TOKEN_IDENTIFIER,  // Exemple : variable, fn
  TOKEN_KEYWORD,     // Exemple : return, if
  TOKEN_ASSIGN,      // =
  TOKEN_SEMICOLON,   // ;
  TOKEN_UNKNOWN      // Caractère non reconnu
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

// Fonction utilitaire pour vérifier si une chaîne est un mot-clé
static bool
is_keyword (const char *str) {
  return (strcmp (str, "return") == 0 || strcmp (str, "if") == 0 || strcmp (str, "fn") == 0);
}

// Fonction principale du Lexer : extrait le prochain Token du flux générique
Token
next_token (StreamReader *stream) {
  int c = stream_look_ahead (stream, 1);

  // 1. Ignorer les espaces blancs
  while (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
    stream_get (stream);
    c = stream_look_ahead (stream, 1);
  }

  // 2. Gestion de la fin de fichier
  if (c == EOF) {
    return (Token){ .type = TOKEN_EOF, .value = strdup ("EOF") };
  }

  // 3. Identification des NOMBRES (Ex: 42)
  if (isdigit (c)) {
    size_t len = 0;
    // Utilisation du look-ahead pour mesurer la taille du nombre
    while (isdigit (stream_look_ahead (stream, len + 1))) {
      len++;
    }

    // Allocation et extraction
    char *buffer = malloc ((len + 1) * sizeof (*buffer));
    for (size_t i = 0; i < len; i++) {
      buffer[i] = (char)stream_get (stream);
    }
    buffer[len] = '\0';

    return (Token){ .type = TOKEN_INT_LITERAL, .value = buffer };
  }

  // 4. Identification des IDENTIFIANTS et MOTS-CLÉS
  if (isalpha (c) || c == '_') {
    size_t len = 0;
    int ahead = stream_look_ahead (stream, len + 1);
    while (isalnum (ahead) || ahead == '_') {
      len++;
      ahead = stream_look_ahead (stream, len + 1);
    }

    char *buffer = malloc ((len + 1) * sizeof (*buffer));
    for (size_t i = 0; i < len; i++) {
      buffer[i] = (char)stream_get (stream);
    }
    buffer[len] = '\0';

    // Distinction entre mot-clé et identifiant générique
    TokenType type = is_keyword (buffer) ? TOKEN_KEYWORD : TOKEN_IDENTIFIER;
    return (Token){ .type = type, .value = buffer };
  }

  // 5. Identification des symboles simples (Opérateurs / Ponctuation)
  if (c == '=') {
    stream_get (stream);
    return (Token){ .type = TOKEN_ASSIGN, .value = strdup ("=") };
  }
  if (c == ';') {
    stream_get (stream);
    return (Token){ .type = TOKEN_SEMICOLON, .value = strdup (";") };
  }

  // 6. Caractère inconnu
  stream_get (stream);
  char unknown_str[2] = { (char)c, '\0' };
  return (Token){ .type = TOKEN_UNKNOWN, .value = strdup (unknown_str) };
}

// ========================== test
static const char *
token_type_to_string (TokenType type) {
  switch (type) {
  case TOKEN_EOF:
    return "EOF";
  case TOKEN_INT_LITERAL:
    return "INT_LITERA";
  case TOKEN_IDENTIFIER:
    return "IDENTIFIER";
  case TOKEN_KEYWORD:
    return "KEYWORD";
  case TOKEN_ASSIGN:
    return "ASSIGN";
  case TOKEN_SEMICOLON:
    return "SEMICOLON";
  case TOKEN_UNKNOWN:
  default:
    return "UNKNOWN";
  }
}

int
main (void) {
  // Code source à analyser
  char *source_code = "return 42; score = 100;";

  // Initialisation du Reader générique basé sur la chaîne
  Reader base_reader = create_str_reader (source_code, 0);

  // Initialisation du flux avec look-ahead
  StreamReader *stream = stream_create (base_reader);

  printf ("Analyse lexicale de : \"%s\"\n\n", source_code);

  Token tok;
  do {
    tok = next_token (stream);
    printf ("[%s] -> \"%s\"\n", token_type_to_string (tok.type), tok.value);
    token_free (tok); // Libère la chaîne du token courant
  } while (tok.type != TOKEN_EOF);

  stream_destroy (stream);
  return EXIT_SUCCESS;
}
