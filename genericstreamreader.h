#ifndef __GENERIC_STREAM_READER_H__
#define __GENERIC_STREAM_READER_H__

#include <stddef.h>

#define stream_get_and_forget(stream)                  \
  do {                                                 \
    StreamReader *__gsr__ = (stream);                  \
    stream_forgetter (__gsr__) (stream_get (__gsr__)); \
  } while (0)

typedef void (*free_prototype) (void *);
typedef struct {
  void *instance;                    // Pointer to the concrete structure (FILE*, string, etc.)
  free_prototype free_instance_fn;   // Optional deallocator to free memory allocated by the reader used by stream_create.
  void *(*read_fn) (void *instance); // Function to read a character from the instance (returns the character read, or EOF in the event of an end-of-stream or error.)
  free_prototype forget_read_fn;     // Optional character deallocator to free memory allocated by read_fn.
} Reader;

typedef struct StreamReader StreamReader;

// Create a stream for reading.
__attribute__ ((warn_unused_result))
StreamReader *
stream_create (Reader reader);

// Destroy a stream.
void stream_destroy (StreamReader *stream);

// Read a character from the stream.
// Returns:
// - 0 if the end of the stream is reached or an error occurs;
// - errno is set if an error occurs while reading from the stream via stream->read_fn;
// - otherwise, a pointer to the next character from the stream, consuming it.
__attribute__ ((warn_unused_result)) void *stream_get (StreamReader *stream);

// Look-ahead by n characters.
// Returns:
// - 0 if the end of the stream is reached or an error occurs;
// - errno = ENOMEM in the event of a memory allocation error;
// - errno is set if an error occurs while reading from the stream via stream->read_fn;
// - otherwise, a pointer to the nth (n > 0) subsequent character, without consuming it from the stream.
void *stream_look_ahead (StreamReader *stream, size_t n);

// Returns the function forget_fn of the reader associated to the stream.
// This returned function must be applied to the result of a call to stream_get to avoid memory leaks.
free_prototype stream_forgetter (StreamReader *stream);

//================== HELPERS  for stream_create ======================
#include <stdio.h>
#include <wchar.h>

Reader ch_file_reader (FILE *f);
Reader wc_file_reader (FILE *f);
Reader str_reader (char *source_str);
Reader wcs_reader (wchar_t *source_str);

#endif
