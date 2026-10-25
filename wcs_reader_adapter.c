#include "wcs_reader_adapter.h"
#include <stdlib.h>

// ==================== deux implementations clasiques de l'interface
// --- IMPLEMENTATION 1: Lecture depuis un Fichier (FILE*) ---
static wint_t
file_read_adapter (void *instance) {
  return fgetwc ((FILE *)instance);
}

Reader
create_file_reader (FILE *f) {
  return (Reader){ .instance = f, .read_fn = file_read_adapter, .free_fn = 0 };
}

// --- IMPLEMENTATION 2: Lecture depuis une Chaîne en Mémoire ---
struct WCStringReaderCtx {
  wchar_t *str;
  size_t cursor;
  int free_after_use;
};

static wint_t
wcs_read_adapter (void *instance) {
  struct WCStringReaderCtx *ctx = instance;
  wchar_t c = ctx->str[ctx->cursor];
  if (c == L'\0')
    return WEOF; // Fin de chaîne
  ctx->cursor++;
  return (wint_t)c;
}

static void
wcs_free_adapter (void *instance) {
  struct WCStringReaderCtx *ctx = instance;
  if (ctx->free_after_use)
    free (ctx->str);
  free (ctx);
}

Reader
create_wcs_reader (wchar_t *source_str, int free_after_use) {
  struct WCStringReaderCtx *ctx = malloc (sizeof (*ctx));
  ctx->str = source_str;
  ctx->free_after_use = free_after_use;
  ctx->cursor = 0;
  return (Reader){ .instance = ctx, .read_fn = wcs_read_adapter, .free_fn = wcs_free_adapter };
}
