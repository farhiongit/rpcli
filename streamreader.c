#undef NDEBUG
#include "streamreader.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static const size_t INITIAL_CAPACITY = 1;

struct StreamReader {
  Reader reader;   // L'interface de lecture générique
  SR_TYPE *buffer; // Tampon circulaire
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
    SR_TYPE c = stream->reader.read_fn (stream->reader.instance);
    if (c == SR_EOF) {
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

SR_TYPE
stream_look_ahead (StreamReader *stream, size_t distance) {
  assert (distance > 0);
  if (!stream_fill (stream, distance))
    return SR_EOF;

  size_t index = (stream->head + distance - 1) % stream->capacity;
  return stream->buffer[index];
}

SR_TYPE
stream_get (StreamReader *stream) {
  if (!stream_fill (stream, 1))
    return SR_EOF;

  SR_TYPE c = stream->buffer[stream->head];
  stream->head = (stream->head + 1) % stream->capacity;
  assert (stream->size);
  stream->size--;
  return c;
}
