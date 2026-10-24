#undef NDEBUG
#include "streamreader.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static const size_t INITIAL_CAPACITY = 1;

struct StreamReader {
  Reader reader;   // L'interface de lecture générique
  wchar_t *buffer; // Tampon circulaire
  size_t capacity;
  size_t head;
  size_t tail;
  size_t size;
  bool is_eof;
};

__attribute__ ((warn_unused_result)) StreamReader *
stream_create (Reader reader) {
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

void
stream_destroy (StreamReader *stream) {
  if (stream->reader.free_fn)
    stream->reader.free_fn (stream->reader.instance);
  free (stream->buffer);
  free (stream);
}

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
    if (!stream->reader.read_fn || !stream->reader.instance)
      return (errno = ENODATA), false;
    wint_t c = stream->reader.read_fn (stream->reader.instance);
    if (c == WEOF) {
      stream->is_eof = true;
      break;
    }

    assert (stream->size < stream->capacity);
    stream->buffer[stream->tail] = (wchar_t)c;
    stream->tail = (stream->tail + 1) % stream->capacity;
    stream->size++;
  } // while (!stream->is_eof && stream->size < need_distance)
  return !stream->is_eof;
}

wint_t
stream_look_ahead (StreamReader *stream, size_t distance) {
  assert (distance > 0);
  if (!stream_fill (stream, distance))
    return WEOF;

  size_t index = (stream->head + distance - 1) % stream->capacity;
  return (wint_t)stream->buffer[index];
}

wint_t
stream_get (StreamReader *stream) {
  if (!stream_fill (stream, 1))
    return WEOF;

  wchar_t c = stream->buffer[stream->head];
  stream->head = (stream->head + 1) % stream->capacity;
  assert (stream->size);
  stream->size--;
  return (wint_t)c;
}

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
static wint_t
string_read_adapter (void *instance) {
  StringReaderCtx *ctx = (StringReaderCtx *)instance;
  wchar_t c = ctx->str[ctx->cursor];
  if (c == '\0')
    return WEOF; // Fin de chaîne
  ctx->cursor++;
  return (wint_t)c;
}

Reader
create_string_reader (StringReaderCtx *ctx, wchar_t *source_str) {
  ctx->str = source_str;
  ctx->cursor = 0;
  return (Reader){ .instance = ctx, .read_fn = string_read_adapter, .free_fn = 0 };
}
