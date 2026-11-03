#ifndef __GENERIC_STREAM_READER_H__
#define __GENERIC_STREAM_READER_H__

#include <stdio.h>
#include <wchar.h>

typedef void (*free_prototype) (void *instance);
typedef struct {
  void *instance;                    // Pointer to the concrete structure (FILE*, string, etc.)
  void *(*read_fn) (void *instance); // Function to read a character from the instance (returns the character read, or EOF in the event of an end-of-stream or error.)
  free_prototype forget_fn;          // Optional character deallocator to free memory allocated by read_fn.
  free_prototype free_fn;            // Optional deallocator to free memory allocated by the reader used by stream_create.
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
// - EOF if the end of the stream is reached or an error occurs;
// - errno is set if an error occurs while reading from the stream via stream->read_fn;
// - otherwise, the next character from the stream, consuming it.
__attribute__ ((warn_unused_result)) void *stream_get (StreamReader *stream);

// Look-ahead by n characters.
// Returns:
// - EOF if the end of the stream is reached or an error occurs;
// - errno = ENOMEM in the event of a memory allocation error;
// - errno is set if an error occurs while reading from the stream via stream->read_fn;
// - otherwise, the nth (n > 0) subsequent character, without consuming it from the stream.
void *stream_look_ahead (StreamReader *stream, size_t n);
free_prototype stream_forgetter (StreamReader *stream);
//================== HELPERS ======================
Reader ch_file_reader (FILE *f);
Reader wc_file_reader (FILE *f);
Reader str_reader (char *source_str);
Reader wcs_reader (wchar_t *source_str);
#endif
