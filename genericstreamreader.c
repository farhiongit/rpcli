#undef NDEBUG
#include "genericstreamreader.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static const size_t INITIAL_CAPACITY = 1;

struct StreamReader {
  Reader reader; // L'interface de lecture générique
  void **buffer; // Tampon circulaire
  size_t capacity;
  size_t head;
  size_t tail;
  size_t size;
  bool is_eof;
};

static bool
stream_resize (StreamReader *stream) {
  void *old_buffer = stream->buffer;
  size_t new_capacity = stream->capacity * 2;
  if (!((stream->buffer = realloc (stream->buffer, new_capacity * sizeof (*stream->buffer)))))
    return (stream->buffer = old_buffer), (errno = ENOMEM), false;

  // Réarrangement
  if (stream->tail <= stream->head) {
    for (size_t i = stream->head; i < stream->capacity /* = old capacity */; i++)
      stream->buffer[i + stream->capacity] = stream->buffer[i];
    stream->head += stream->capacity;
  }

  stream->capacity = new_capacity;
  return true;
}

static bool
stream_fill (StreamReader *stream, size_t need_distance) {
  assert (need_distance);
  while (!stream->is_eof && stream->size < need_distance) {
    assert (stream->size <= stream->capacity);
    if (stream->size == stream->capacity) { // full
      if (!stream_resize (stream))
        return false;
    }

    // Appel à l'interface générique (au lieu de fgetwc)
    void *c = stream->reader.read_fn (stream->reader.instance);
    if (c == 0) {
      stream->is_eof = true;
      break;
    }

    assert (stream->size < stream->capacity);
    stream->buffer[stream->tail] = c;
    stream->tail = (stream->tail + 1) % stream->capacity;
    stream->size++;
  } // while (!stream->is_eof && stream->size < need_distance)
  return !stream->is_eof;
}

static void
stream_drain (StreamReader *stream) {
  if (stream->reader.forget_fn)
    while (stream->size)
      stream->reader.forget_fn (stream_get (stream));
}

static void
_noop (void *p) {
  (void)p;
}

__attribute__ ((warn_unused_result)) StreamReader *
stream_create (Reader reader) {
  if (!reader.read_fn || !reader.instance)
    return (errno = ENODATA), NULL;

  StreamReader *stream = malloc (sizeof (*stream));
  if (!stream)
    return (errno = ENOMEM), NULL;

  stream->reader = reader;
  stream->capacity = INITIAL_CAPACITY ? INITIAL_CAPACITY : 1;
  stream->buffer = malloc (stream->capacity * sizeof (*stream->buffer));
  stream->head = 0;
  stream->tail = 0;
  stream->size = 0;
  stream->is_eof = false;

  if (!stream->buffer) {
    free (stream);
    return (errno = ENOMEM), NULL;
  }
  return stream;
}

__attribute__ ((warn_unused_result)) void *
stream_get (StreamReader *stream) {
  if (!stream_fill (stream, 1))
    return 0;

  void *c = stream->buffer[stream->head];
  stream->head = (stream->head + 1) % stream->capacity;
  assert (stream->size);
  stream->size--;
  return c;
}

void *
stream_look_ahead (StreamReader *stream, size_t distance) {
  assert (distance > 0);
  if (!stream_fill (stream, distance))
    return 0;

  size_t index = (stream->head + distance - 1) % stream->capacity;
  return stream->buffer[index];
}

void
stream_destroy (StreamReader *stream) {
  stream_drain (stream);
  if (stream->reader.free_fn)
    stream->reader.free_fn (stream->reader.instance);
  free (stream->buffer);
  free (stream);
}

free_prototype
stream_forgetter (StreamReader *stream) {
  if (stream->reader.forget_fn)
    return stream->reader.forget_fn;
  else
    return _noop;
}

//================== HELPERS ======================
static void *
ch_file_reader_adapter (void *f) {
  int ch = fgetc (f);
  if (ch == EOF)
    return 0;
  int *c = malloc (sizeof (c));
  assert (c);
  *c = ch;
  return c;
}

Reader
ch_file_reader (FILE *f) {
  return (Reader){ .instance = f, .read_fn = ch_file_reader_adapter, .forget_fn = free };
}

static void *
wc_file_reader_adapter (void *f) {
  wint_t wc = fgetwc (f);
  if (wc == WEOF)
    return 0;
  wint_t *c = malloc (sizeof (c));
  assert (c);
  *c = wc;
  return c;
}

Reader
wc_file_reader (FILE *f) {
  return (Reader){ .instance = f, .read_fn = wc_file_reader_adapter, .forget_fn = free };
}

struct StringReaderCtx {
  void *str;
  size_t cursor;
};

static void *
str_read_adapter (void *instance) {
  struct StringReaderCtx *ctx = instance;
  char *c = ctx->str;
  c += ctx->cursor;
  if (*c == '\0')
    return 0; // Fin de chaîne
  ctx->cursor++;
  return c;
}

Reader
str_reader (char *source_str) {
  struct StringReaderCtx *ctx = malloc (sizeof (*ctx));
  ctx->str = source_str;
  ctx->cursor = 0;
  return (Reader){ .instance = ctx, .read_fn = str_read_adapter, .free_fn = free };
}

static void *
wcs_read_adapter (void *instance) {
  struct StringReaderCtx *ctx = instance;
  wchar_t *c = ctx->str;
  c += ctx->cursor;
  if (*c == L'\0')
    return 0; // Fin de chaîne
  ctx->cursor++;
  return c;
}

Reader
wcs_reader (wchar_t *source_str) {
  struct StringReaderCtx *ctx = malloc (sizeof (*ctx));
  ctx->str = source_str;
  ctx->cursor = 0;
  return (Reader){ .instance = ctx, .read_fn = wcs_read_adapter, .free_fn = free };
}
