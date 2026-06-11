/* tvhepg_stubs.c — no-op / dummy symbols for the standalone EPG extractor.
 *
 * These resolve tvheadend leaf subsystems we deliberately do NOT link
 * (dvr, htsp, bouquet, autorec/timerec, access, comet, notify, imagecache,
 * service_mapper, subscriptions, timers, fb/net I/O). They fire only on
 * channel/dvr/htsp lifecycle events the offline tool never triggers.
 *
 * NOTE: this file includes NO tvheadend headers on purpose — symbols are
 * resolved by name at link time, so plain definitions avoid prototype
 * clashes. Functions whose RESULT the harness actually depends on
 * (mpegts_*_find_*, mpegts_table_add, epggrab_queue_data, the module
 * create/find, the mutex/clock globals) are implemented for real in
 * libtvhepg.c and are intentionally absent here.
 */

#define STUB(name) long name(void) { return 0; }

/* tvhlog replaced by no-ops (we don't init the log subsystem). */
void _tvhlog(const char *f, int l, int sev, int sub, const char *fmt, ...) {}
void _tvhlog_hexdump(int a, const void *b, long c, ...) {}

/* --- dvr / autorec / timerec --- */
STUB(dvr_autorec_check_event)
STUB(dvr_cutpoint_delete_files)
STUB(dvr_destroy_by_channel)
STUB(dvr_entry_changed)
STUB(dvr_entry_is_upcoming)
STUB(dvr_event_removed)
STUB(dvr_event_replaced)
STUB(dvr_event_running)
STUB(dvr_event_updated)
STUB(autorec_destroy_by_channel)
STUB(autorec_destroy_by_channel_tag)
STUB(timerec_destroy_by_channel)

/* --- htsp --- */
STUB(htsp_channel_add)
STUB(htsp_channel_delete)
STUB(htsp_channel_update)
STUB(htsp_channel_update_nownext)
STUB(htsp_event_add)
STUB(htsp_event_delete)
STUB(htsp_event_update)
STUB(htsp_tag_add)
STUB(htsp_tag_delete)
STUB(htsp_tag_update)

/* --- bouquet / access / comet / notify / imagecache --- */
STUB(bouquet_class_get_list)
STUB(bouquet_destroy_by_channel_tag)
STUB(bouquet_get_channel_number)
STUB(access_destroy_by_channel_tag)
STUB(comet_mailbox_add_logmsg)
STUB(notify_by_msg)
STUB(notify_delayed)
STUB(notify_reload)
STUB(imagecache_get_id)
STUB(imagecache_get_propstr)

/* --- service helpers (channel naming/quality) --- */
STUB(service_get_channel_epgid)
STUB(service_get_channel_icon)
STUB(service_get_channel_name)
STUB(service_get_channel_number)
STUB(service_get_source)
STUB(service_is_fhdtv)
STUB(service_is_hdtv)
STUB(service_is_sdtv)
STUB(service_is_uhdtv)
STUB(service_mapper_create)
STUB(service_remove_subscriber)

/* --- subscriptions --- */
STUB(subscription_add_bytes_in)
STUB(subscription_add_bytes_out)

/* --- epggrab ota bits we don't use --- */
STUB(epggrab_channel_add)
STUB(epggrab_channel_map)
STUB(epggrab_channel_rem)
STUB(epggrab_ota_complete)
STUB(epggrab_ota_find_map)
STUB(epggrab_ota_find_mux)
STUB(epggrab_ota_free_eit_plist)
STUB(epggrab_ota_register)
STUB(epggrab_ota_service_add)
STUB(epggrab_ota_service_del)

/* --- timers / tasklets --- */
STUB(gtimer_arm_absn)
STUB(gtimer_arm_rel)
STUB(gtimer_disarm)
STUB(mtimer_arm_abs)
STUB(mtimer_arm_rel)
STUB(mtimer_disarm)
STUB(tasklet_arm_alloc)

/* --- file/net I/O (settings persistence, sockets) --- */
STUB(tcp_get_ip_from_str)
STUB(tvh_gzip_deflate_fd_header)
STUB(tvh_gzip_deflate)
STUB(tvh_gzip_inflate)

/* --- reached only via _eit_callback (bypassed) --- */
STUB(mpegts_mux_find_service)
STUB(mpegts_network_find_mux)
STUB(mpegts_network_find_active_service)
STUB(mpegts_mux_find_subscription_by_name)
STUB(mpegts_table_add)
STUB(epggrab_queue_data)

/* --- misc --- */
STUB(dvb_bat_callback)
STUB(mpegts_mux_set_epg_module)
STUB(my_str2double)
STUB(my_double2str)
STUB(tvh_gettext_lang)
STUB(tvh_str_update)
STUB(config_get_language)
STUB(doexit)
STUB(htsmsg_binary2_deserialize0)
STUB(htsmsg_binary2_serialize0)
STUB(htsmsg_binary2_deserialize)
STUB(htsmsg_binary2_serialize)
STUB(epggrab_save_timer)

/* --- data symbols referenced as globals (zeroed dummies) --- */
int  tvhlog_level;
int  tvhdbg;
char *tvheadend_cwd = (char *)"/";
char config[8192];
char dvrentries[256];
char bouquet_class[1024];
char dvb_network_cablecard_class[1024];
char dvb_network_dvbc_class[1024];
char epggrab_channel_class[1024];
char epggrab_mod_ota_scraper_class[1024];
char epggrab_ota_genre_translation[256];
char tvh_doc_channel_class[256];
char tvh_doc_channeltag_class[256];
char tvh_doc_debugging_class[256];
char tvh_doc_memoryinfo_class[256];
char tvh_doc_ratinglabel_class[256];
char tvh_doc_runningstate_property[256];
