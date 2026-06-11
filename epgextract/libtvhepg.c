/* libtvhepg — drive tvheadend's real EIT parser (eit.c/epg.c) over a .ts file.
 *
 * Strategy: skip the OTA/input stack entirely. Build the eit_data_t header that
 * eit.c's _eit_callback would have produced, then call the eit module's
 * ops->process_data directly — that runs the real _eit_process_event ->
 * epg_broadcast_set_* into a synthetic channel's ch_epg_schedule, which we walk
 * to emit JSON. ARIB decoding fires because we set the per-event charset to
 * ARIB-STD-B24.
 */
#include "tvheadend.h"
#include "clock.h"
#include "idnode.h"
#include "settings.h"
#include "lang_str.h"
#include "epg.h"
#include "epggrab.h"
#include "epggrab/private.h"
#include "channels.h"
#include "service.h"
#include "input.h"
#include "input/mpegts.h"
#include "input/mpegts/dvb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libtvhepg.h"

/* ---- eit.c private header layout (must match epggrab/module/eit.c) ---- */
typedef struct eit_data {
  tvh_uuid_t svc_uuid;
  uint16_t   onid;
  uint16_t   tsid;
  int        tableid;
  int        sect;
  int        local_time;
  uint16_t   charset_len;
  uint16_t   cridauth_len;
  uint8_t    data[0];
} eit_data_t;

/* ---- glue globals the linked tvheadend objects expect ---- */
tvh_mutex_t       global_lock;
tvh_mutex_t       fork_lock;
time_t            __gdispatch_clock;
time_t            __mdispatch_clock;
int               tvheadend_running = 1;
int               epggrab_ota_running = 1;
epggrab_conf_t    epggrab_conf;
const idclass_t   service_class = { .ic_class = "service",
                                    .ic_caption = N_("Service") };
const idclass_t   mpegts_service_class = { .ic_class = "mpegts_service",
                                           .ic_caption = N_("MPEG-TS Service"),
                                           .ic_super = &service_class };

/* ---- captured eit module + synthetic object graph ---- */
static epggrab_module_t *g_mod;
static channel_t        *g_ch;
static mpegts_service_t *g_svc;
static const char       *g_charset = "ARIB-STD-B24";
static int               g_present_eid = -1;
static int               g_target_sid = -1;   /* recorded service id (from PAT) */

/* epggrab_module_ota_create: mimic module.c, capture the module, no idnode. */
epggrab_module_ota_t *epggrab_module_ota_create
  ( epggrab_module_ota_t *skel, const char *id, int subsys, const char *saveid,
    const char *name, int priority, const idclass_t *idclass,
    const epggrab_ota_module_ops_t *ops )
{
  if (!skel) skel = calloc(1, sizeof(epggrab_module_ota_t));
  skel->id           = strdup(id);
  skel->subsys       = subsys;
  skel->saveid       = saveid ? strdup(saveid) : NULL;
  skel->enabled      = 1;
  skel->type         = EPGGRAB_OTA;
  skel->activate     = ops->activate;
  skel->start        = ops->start;
  skel->stop         = ops->stop;
  skel->handlers     = ops->handlers;
  skel->done         = ops->done;
  skel->tune         = ops->tune;
  skel->process_data = ops->process_data;
  skel->opaque       = ops->opaque;
  if (!g_mod) g_mod = (epggrab_module_t *)skel;   /* the base "eit" module */
  return skel;
}

epggrab_module_t *epggrab_module_find_by_id(const char *id)
{
  if (g_mod && !strcmp(g_mod->id, id)) return g_mod;
  return NULL;
}

/* mpegts_* lookups + epggrab_queue_data are reached only via _eit_callback,
 * which we bypass — defined as header-free no-ops in tvhepg_stubs.c. */

/* ------------------------------------------------------------------ */

static void write_eit_config(const char *dir)
{
  char path[1024];
  snprintf(path, sizeof(path), "%s/epggrab/eit", dir);
  /* mkdir -p */
  char cmd[1100];
  snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", path);
  if (system(cmd)) {}
  snprintf(path, sizeof(path), "%s/epggrab/eit/config", dir);
  FILE *f = fopen(path, "w");
  if (f) {
    fputs("{ \"eit\": { \"name\": \"EIT\", \"prio\": 1 } }\n", f);
    fclose(f);
  }
}

int tvhepg_init(const char *charset)
{
  if (charset) g_charset = charset;
  /* gclk()=0 so events from an old capture aren't treated as expired and
   * dropped by epg_channel_ignore_broadcast. */
  __gdispatch_clock = 0;
  __mdispatch_clock = time(NULL);

  tvh_mutex_init(&global_lock, NULL);

  /* settings dir with a minimal eit module config */
  char tmpl[] = "/tmp/tvhepgXXXXXX";
  char *dir = mkdtemp(tmpl);
  if (!dir) return -1;
  write_eit_config(dir);

  tvh_mutex_lock(&global_lock);
  idnode_init();
  hts_settings_init(dir);
  epg_init();
  eit_init();
  tvh_mutex_unlock(&global_lock);

  if (!g_mod) { fprintf(stderr, "tvhepg: eit module not created\n"); return -1; }

  /* synthetic network/mux/service/channel */
  mpegts_network_t *net = calloc(1, sizeof(mpegts_network_t));
  mpegts_mux_t     *mux = calloc(1, sizeof(mpegts_mux_t));
  g_svc = calloc(1, sizeof(mpegts_service_t));
  g_ch  = calloc(1, sizeof(channel_t));

  tvh_mutex_lock(&global_lock);
  idclass_register(&service_class);
  idclass_register(&mpegts_service_class);
  idclass_register(&channel_class);
  mux->mm_network = net;
  g_svc->s_dvb_mux = mux;
  g_svc->s_dvb_svcname = strdup("svc");
  g_svc->s_dvb_charset = strdup(g_charset);
  idnode_insert(&g_svc->s_id, "00000000000000000000000000000a01",
                &mpegts_service_class, 0);
  idnode_insert(&g_ch->ch_id, "00000000000000000000000000000c01",
                &channel_class, 0);
  g_ch->ch_enabled = 1;
  idnode_list_link(&g_svc->s_id, &g_svc->s_channels,
                   &g_ch->ch_id, &g_ch->ch_services, g_svc, 1);
  tvh_mutex_unlock(&global_lock);
  return 0;
}

/* Build eit_data_t + event bytes and run the real parser synchronously. */
static void process_section(int tableid, const uint8_t *p, int len)
{
  if (len < 11 + 12) return;                 /* EIT hdr + >=1 event */
  uint16_t tsid = (p[5] << 8) | p[6];
  uint16_t onid = (p[7] << 8) | p[8];
  int sect = p[3];
  const uint8_t *ev = p + 11;
  int evlen = len - 11;

  /* EIT p/f section 0 = present (currently-running) event; remember its id so
   * --now can flag THE running programme (vs the following one). */
  if (tableid == 0x4e && sect == 0)
    g_present_eid = (ev[0] << 8) | ev[1];

  size_t clen = strlen(g_charset) + 1;
  size_t hlen = sizeof(eit_data_t) + clen;   /* cridauth_len = 0 */
  uint8_t *buf = malloc(hlen + evlen);
  eit_data_t *ed = (eit_data_t *)buf;
  memset(ed, 0, sizeof(*ed));
  ed->svc_uuid    = g_svc->s_id.in_uuid;
  ed->onid        = onid;
  ed->tsid        = tsid;
  ed->tableid     = tableid;
  ed->sect        = sect;
  ed->local_time  = 0;
  ed->cridauth_len = 0;
  ed->charset_len  = clen;
  memcpy(ed->data, g_charset, clen);
  memcpy(buf + hlen, ev, evlen);

  g_mod->process_data(g_mod, buf, hlen + evlen);
  free(buf);
}

/* PAT programs collected from the file. The recorded service is the one whose
 * PMT PID actually carries packets (a CS transport lists every co-tenant in the
 * PAT, but only the recorded service's PIDs are present in the file). */
#define MAXPROG 64
static int g_prog[MAXPROG], g_pmtpid[MAXPROG], g_nprog;
static void pat_cb(mpegts_psi_table_t *mt, const uint8_t *ptr, int len)
{
  if (ptr[-3] != 0x00 || g_nprog) return;               /* PAT only, once */
  for (int i = 5; i + 4 <= len && g_nprog < MAXPROG; i += 4) {
    int prog = (ptr[i] << 8) | ptr[i + 1];
    int pid  = ((ptr[i + 2] & 0x1f) << 8) | ptr[i + 3];
    if (prog != 0) { g_prog[g_nprog] = prog; g_pmtpid[g_nprog] = pid; g_nprog++; }
  }
}

/* dvb_table_parse callback: full=1 -> ptr already at the section body (svc id) */
static tvhepg_mode_t g_mode;
static void psi_cb(mpegts_psi_table_t *mt, const uint8_t *ptr, int len)
{
  int tableid = ptr[-3];
  if (tableid < 0x4e || tableid > 0x6f) return;
  if (g_mode == TVHEPG_NOW && tableid != 0x4e) return;
  /* only the recorded service (a CS transport carries p/f for many services) */
  if (g_target_sid >= 0 && (((ptr[0] << 8) | ptr[1]) != g_target_sid)) return;
  process_section(tableid, ptr, len);
}

int tvhepg_feed_file(const char *path, tvhepg_mode_t mode)
{
  g_mode = mode;
  FILE *f = fopen(path, "rb");
  if (!f) { fprintf(stderr, "tvhepg: cannot open %s\n", path); return -1; }

  /* Pre-scan the file start: read the PAT and count packets per PID, then pick
   * the recorded service = the PAT program whose PMT PID actually has packets.
   * Lets us ignore co-tenant services' EIT on a shared (CS) transport. */
  {
    mpegts_psi_table_t pt;
    dvb_table_parse_init(&pt, "pat", LS_TBL_BASE, 0, 0, 0, NULL);
    enum { PB = 188 * 4096 };
    uint8_t *pb = malloc(PB);
    uint32_t *pidcnt = calloc(8192, sizeof(uint32_t));
    size_t pn; long blocks = 0;
    while (blocks < 12 && (pn = fread(pb, 1, PB, f)) >= 188) {  /* ~18MB window */
      blocks++;
      for (size_t off = 0; off + 188 <= pn; off += 188) {
        uint8_t *pkt = pb + off;
        if (pkt[0] != 0x47) continue;
        int pid = ((pkt[1] & 0x1f) << 8) | pkt[2];
        pidcnt[pid]++;
        if (pid == 0) dvb_table_parse(&pt, "pat", pkt, 188, 1, 1, pat_cb);
      }
    }
    uint32_t best = 0;
    for (int i = 0; i < g_nprog; i++)
      if (pidcnt[g_pmtpid[i]] > best) { best = pidcnt[g_pmtpid[i]]; g_target_sid = g_prog[i]; }
    if (g_target_sid < 0 && g_nprog) g_target_sid = g_prog[0];   /* fallback */
    free(pb); free(pidcnt);
    dvb_table_parse_done(&pt);
    rewind(f);
  }

  mpegts_psi_table_t mt;
  dvb_table_parse_init(&mt, "eit", LS_TBL_EIT, DVB_EIT_PID, 0, 0, NULL);

  /* --now: a recording brackets the target programme with pre/post-roll, so the
   * p/f "present" at the START is the previous show. Sample at the file MIDPOINT
   * instead — there the present event is the recorded programme. --all scans the
   * whole file. */
  long start_off = 0;
  if (g_mode == TVHEPG_NOW) {
    if (!fseek(f, 0, SEEK_END)) {
      long sz = ftell(f);
      if (sz > 0) start_off = (sz / 2) / 188 * 188;   /* packet-aligned */
    }
    fseek(f, start_off, SEEK_SET);
  }

  /* Block reads keep multi-GB files fast (one read per ~1.5MB, not per packet). */
  enum { BLK = 188 * 8192 };
  uint8_t *buf = malloc(BLK);
  long n12 = 0, firstev = -1;
  size_t n;
  int stop = 0;
  while (!stop && (n = fread(buf, 1, BLK, f)) >= 188) {
    for (size_t off = 0; off + 188 <= n; off += 188) {
      uint8_t *pkt = buf + off;
      if (pkt[0] != 0x47) continue;
      int pid = ((pkt[1] & 0x1f) << 8) | pkt[2];
      if (pid != DVB_EIT_PID) continue;
      /* No outer global_lock: eit's process_data path locks it itself
       * (non-recursive tvh_mutex would otherwise deadlock). */
      dvb_table_parse(&mt, "eit", pkt, 188, 1, 1, psi_cb);
      n12++;
      if (g_mode == TVHEPG_NOW) {
        /* once we have the present p/f, read a short margin to capture the
         * following too, then stop. */
        if (firstev < 0 && g_present_eid >= 0 &&
            RB_FIRST(&g_ch->ch_epg_schedule)) firstev = n12;
        if ((firstev >= 0 && n12 - firstev > 300) || n12 > 200000) { stop = 1; break; }
      }
    }
  }
  free(buf);
  fclose(f);
  dvb_table_parse_done(&mt);
  return 0;
}

/* ---------------- JSON ---------------- */
struct sb { char *p; size_t len, cap; };
static void sb_putc(struct sb *s, char c)
{
  if (s->len + 1 >= s->cap) { s->cap = s->cap ? s->cap * 2 : 4096; s->p = realloc(s->p, s->cap); }
  s->p[s->len++] = c;
}
static void sb_raw(struct sb *s, const char *str)
{ for (; *str; str++) sb_putc(s, *str); }
static void sb_json_str(struct sb *s, const char *str)
{
  sb_putc(s, '"');
  for (; str && *str; str++) {
    unsigned char c = *str;
    if (c == '"' || c == '\\') { sb_putc(s, '\\'); sb_putc(s, c); }
    else if (c == '\n') sb_raw(s, "\\n");
    else if (c == '\r') sb_raw(s, "\\r");
    else if (c == '\t') sb_raw(s, "\\t");
    else if (c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); sb_raw(s, b); }
    else sb_putc(s, c);
  }
  sb_putc(s, '"');
}
static void sb_kv_str(struct sb *s, const char *k, const char *v, int *first)
{
  if (!v || !*v) return;
  if (!*first) sb_putc(s, ','); *first = 0;
  sb_json_str(s, k); sb_putc(s, ':'); sb_json_str(s, v);
}
static void sb_kv_int(struct sb *s, const char *k, long v, int *first)
{
  if (!*first) sb_putc(s, ','); *first = 0;
  char b[32]; snprintf(b, sizeof(b), "%ld", v);
  sb_json_str(s, k); sb_putc(s, ':'); sb_raw(s, b);
}

char *tvhepg_to_json(tvhepg_mode_t mode, const char *lang, int pretty)
{
  struct sb s = {0};
  epg_broadcast_t *ebc;
  sb_raw(&s, "{\"mode\":");
  sb_json_str(&s, mode == TVHEPG_NOW ? "now" : "all");
  sb_raw(&s, ",\"events\":[");
  int firstev = 1;
  RB_FOREACH(ebc, &g_ch->ch_epg_schedule, sched_link) {
    if (!firstev) sb_putc(&s, ','); firstev = 0;
    sb_putc(&s, '{');
    int f = 1;
    sb_kv_int(&s, "dvb_eid", ebc->dvb_eid, &f);
    if (mode == TVHEPG_NOW && g_present_eid >= 0 && ebc->dvb_eid == g_present_eid) {
      if (!f) sb_putc(&s, ','); f = 0;
      sb_raw(&s, "\"current\":true");
    }
    sb_kv_int(&s, "start", (long)ebc->start, &f);
    sb_kv_int(&s, "stop",  (long)ebc->stop, &f);
    sb_kv_int(&s, "running", ebc->running, &f);
    sb_kv_str(&s, "title",       lang_str_get(ebc->title, lang), &f);
    sb_kv_str(&s, "subtitle",    lang_str_get(ebc->subtitle, lang), &f);
    sb_kv_str(&s, "summary",     lang_str_get(ebc->summary, lang), &f);
    sb_kv_str(&s, "description", lang_str_get(ebc->description, lang), &f);
    if (ebc->epnum.e_num) sb_kv_int(&s, "episode", ebc->epnum.e_num, &f);
    /* genres */
    epg_genre_t *g = LIST_FIRST(&ebc->genre);
    if (g) {
      if (!f) sb_putc(&s, ','); f = 0;
      sb_raw(&s, "\"genres\":[");
      int gf = 1;
      LIST_FOREACH(g, &ebc->genre, link) {
        if (!gf) sb_putc(&s, ','); gf = 0;
        char b[16]; snprintf(b, sizeof(b), "%d", g->code); sb_raw(&s, b);
      }
      sb_putc(&s, ']');
    }
    sb_putc(&s, '}');
  }
  sb_raw(&s, "]}");
  sb_putc(&s, '\n');
  sb_putc(&s, '\0');
  return s.p;
}

void tvhepg_done(void) {}
