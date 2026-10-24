#undef NDEBUG
#include "mongoose.h"
#include "rpgrammar.tab.h"
#include "streamreader.h"
#include <assert.h>
#include <locale.h>

#define _(str) (str) // i18n

// Test with: curl -v --data-raw $'11 50 +\n' http://127.0.0.1:8000/api/rpn

// fmemopen can't be used to read wide-characters.
// The stream returned by fmemopen() is byte-oriented and cannot be changed to wide-character oriented.
// See https://stackoverflow.com/questions/45624195/read-wide-char-from-a-stream-created-with-fmemopen
// Therefore, we use a generic input stream.
static Reader
create_mongoose_reader (const struct mg_str source_str) {
  if (!source_str.buf || !source_str.len)
    return (Reader){ 0 };
  char *str = 0;
  if (source_str.buf[source_str.len - 1] != '\0') {
    str = malloc ((source_str.len + 1) * sizeof (*str));
    assert (str);
    for (size_t i = 0; i < source_str.len; i++)
      str[i] = source_str.buf[i];
    str[source_str.len] = 0;
  } else
    str = strdup (source_str.buf);
  assert (str);
  size_t nbwc = mbstowcs (0, str, 0);
  assert (nbwc != (size_t)-1);
  wchar_t *wcs = malloc ((nbwc + 1) * sizeof (*wcs));
  assert (wcs);
  assert (nbwc == mbstowcs (wcs, str, nbwc + 1));
  free (str);
  Reader r = create_string_reader (wcs, 1 /* wcs will be free'd after use when stream_destroy is called (which calls r.free_fn on r.instance).*/);
  return r;
}

static int
vsnappendf (char *str, size_t size, const char *format, va_list ap) {
  int ret = 0;
  if (size > strlen (str))
    ret = vsnprintf (str + strlen (str), size - strlen (str), format, ap);
  return ret;
}

static int
snappendf (char *str, size_t size, const char *format, ...) {
  va_list ap;
  va_start (ap, format);
  int ret = vsnappendf (str, size, format, ap);
  va_end (ap);
  return ret;
}

static void
RPN_print_integer (integer a, void *result) { snappendf (result, 200, "= %lli [%s]\n", a, _ ("integer")); }

static void
RPN_print_decimal (decimal a, void *result) { snappendf (result, 200, "= %Lg\n", a); }

static void
RPN_print_error (const char *msg, void *result) { snappendf (result, 200, "%s: %s.\n", _ ("ERROR"), msg); }

static void
RPN_print_message (const char *format, va_list ap, void *result) { vsnappendf (result, 200, format, ap); }

// Connection event handler function
static void
http_ev_handler (struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_HTTP_MSG) {                                                                        // New HTTP request received
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;                                  // Parsed HTTP request
    if (mg_match (hm->uri, mg_str ("/api/rpn"), NULL) && !mg_strcmp (hm->method, mg_str ("POST"))) { // REST API call
      char result[200] = { 0 };
      context *env = RPN_init ((struct report){ .integer = RPN_print_integer, .decimal = RPN_print_decimal, .error = RPN_print_error, .message = RPN_print_message, .arg = result });
      assert (env);
      Reader base_reader = create_mongoose_reader (hm->body);
      StreamReader *stream = stream_create (base_reader);
      assert (stream);
      RPN_parse (env, stream);
      mg_http_reply (c, 200, "", "%s\n%s\n", hm->body.buf, result);
      stream_destroy (stream);
      RPN_cleanup (env);
    } else {
      mg_http_reply (c, 400, "", "");
    }
  }
}

int
main (void) {
  setlocale (LC_ALL, "");

#ifdef RPN_DEBUG
#if RPN_DEBUG == 1
  RPN_debug = RPN_DEBUG;
#endif
#endif

  struct mg_mgr mgr;
  mg_mgr_init (&mgr);

  if (mg_http_listen (&mgr, "http://0.0.0.0:8000", http_ev_handler, NULL)) {
    for (;;)
      mg_mgr_poll (&mgr, 1000);
    mg_mgr_free (&mgr);
    return EXIT_SUCCESS;
  } else
    return EXIT_FAILURE;
}
