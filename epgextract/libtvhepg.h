/* libtvhepg — extract EPG from an MPEG-TS file by reusing tvheadend's EIT parser.
 * Single-shot, single-threaded, process-global (not reentrant). */
#ifndef LIBTVHEPG_H__
#define LIBTVHEPG_H__

#include <stddef.h>

typedef enum { TVHEPG_NOW = 0, TVHEPG_ALL = 1 } tvhepg_mode_t;

/* Initialise the embedded tvheadend subsystems + a synthetic
 * network/mux/service/channel. charset defaults to "ARIB-STD-B24" when NULL. */
int  tvhepg_init(const char *charset);

/* Parse a .ts file (only PID 0x12 is inspected). In TVHEPG_NOW mode only
 * EIT p/f (0x4E) is processed. Returns 0 on success. */
int  tvhepg_feed_file(const char *path, tvhepg_mode_t mode);

/* Serialise collected events to a malloc'd UTF-8 JSON string (caller frees). */
char *tvhepg_to_json(tvhepg_mode_t mode, const char *lang, int pretty);

void tvhepg_done(void);

#endif
