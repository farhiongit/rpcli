#ifndef __STREAM_READER_H__
#define __STREAM_READER_H__

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <wchar.h>

typedef struct {
  void *instance;                     // Pointeur vers la structure concrète (FILE*, StringCtx, etc.)
  wint_t (*read_fn) (void *instance); // Fonction pour lire un caractère de l'instance (retourne le caractère lu, ou EOF en cas de fin de flux/erreur.)
  void (*free_fn) (void *instance);   // Désalloue l'instance.
} Reader;

// Déclaration des fonctions du Stream Reader pour référence
typedef struct StreamReader StreamReader;

__attribute__ ((warn_unused_result)) StreamReader *stream_create (Reader reader);
void stream_destroy (StreamReader *stream);

// Retourne :
// - EOF si la fin du stream est atteinte ou en cas de d'erreur ;
// - errno = ENOMEM en cas d'errur de dépassement mémoire,
// - errno est positionné en cas d'erreur de lecture du flux par stream->read_fn ;
// - sinon, le nième (n > 0) caractère suivant, sans le consommer du stream.
wint_t stream_look_ahead (StreamReader *stream, size_t n);
// Retourne :
// - EOF si la fin du stream est atteinte ou en cas de d'erreur ;
// - errno est positionné en cas d'erreur de lecture du flux par stream->read_fn ;
// - sinon, le caractère suivant du stream en le consommant.
wint_t stream_get (StreamReader *stream);

// Helper: create from a FILE
Reader create_file_reader (FILE *f);

// Helper: create from a string
typedef struct {
  wchar_t *str;
  size_t cursor;
} StringReaderCtx;

Reader create_string_reader (StringReaderCtx *ctx, wchar_t *source_str);
#endif
