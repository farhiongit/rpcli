#ifndef _STR_SR__
#define _STR_SR__

#include "streamreader.h"
#include <stdio.h>

// Helper: create from a FILE
Reader create_file_reader (FILE *f); // f is the instance of the returned reader.

// Helper: create from a string
Reader create_str_reader (char *source_str, int free_after_use);
#endif
