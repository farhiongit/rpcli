#include "str_reader_adapter.h"
#include <stdlib.h>

// ==================== deux implementations clasiques de l'interface
// --- IMPLEMENTATION 1: Lecture depuis un Fichier (FILE*) ---
static int
file_read_adapter (void *instance) {
  return fgetc ((FILE *)instance);
}

Reader
create_file_reader (FILE *f) {
  return (Reader){ .instance = f, .read_fn = file_read_adapter, .free_fn = 0 };
}

// --- IMPLEMENTATION 2: Lecture depuis une Chaîne en Mémoire ---
struct StringReaderCtx {
  char *str;
  size_t cursor;
  int free_after_use;
};

static int
str_read_adapter (void *instance) {
  struct StringReaderCtx *ctx = instance;
  char c = ctx->str[ctx->cursor];
  if (c == '\0')
    return EOF; // Fin de chaîne
  ctx->cursor++;
  return (int)c;
}

static void
str_free_adapter (void *instance) {
  struct StringReaderCtx *ctx = instance;
  if (ctx->free_after_use)
    free (ctx->str);
  free (ctx);
}

Reader
create_str_reader (char *source_str, int free_after_use) {
  struct StringReaderCtx *ctx = malloc (sizeof (*ctx));
  ctx->str = source_str;
  ctx->free_after_use = free_after_use;
  ctx->cursor = 0;
  return (Reader){ .instance = ctx, .read_fn = str_read_adapter, .free_fn = str_free_adapter };
}
