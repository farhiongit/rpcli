#ifndef _WCS_SR__
#define _WCS_SR__

#include "streamreader.h"
#include <stdio.h>
#include <wchar.h>

// Helper: create from a FILE
Reader create_file_reader (FILE *f); // f is the instance of the returned reader.

// Helper: create from a string
Reader create_wcs_reader (wchar_t *source_str, int free_after_use);
#endif
