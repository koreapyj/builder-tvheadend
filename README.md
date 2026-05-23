# builder-tvheadend

GitHub Actions builder that compiles [Tvheadend](https://github.com/tvheadend/tvheadend)
into Debian `.deb` packages and publishes them as a GitHub Release.

It holds no Tvheadend source — each build clones Tvheadend fresh into `src/`, applies the
patch series in `patches/`, and builds inside a per-distro Docker container.

## What it builds

| Distribution        | Architectures |
|---------------------|---------------|
| Debian 13 (trixie)  | amd64, arm64  |
| Ubuntu 24.04 (noble)| amd64, arm64  |

Each job produces the `tvheadend` and `tvheadend-dbg` `.deb`s, built *lean* — no static
FFmpeg; transcoding links dynamically against the target distro's FFmpeg libraries.

## Triggering a build

Push a tag of the form **`release-g<commit>`**. The `release-g` prefix is stripped and the
remainder is checked out as a Tvheadend commit, then built and published as a Release:

```sh
git tag release-gf37b7b2cb
git push origin release-gf37b7b2cb
```

Tvheadend has no current version tags (development lives on `master`), so builds are pinned
to a commit hash. The `g<commit>` form mirrors `git describe`, so the tag matches the
package version `…~g<commit>~<codename>`.

You can also run the workflow manually (**Actions → Build Tvheadend → Run workflow**) with a
`ref` input — any branch, tag, or commit, default `master`. Manual runs upload artifacts
only; they do not create a Release.

## Patches

`patches/` holds an OpenWrt-style quilt series of numbered `NNN-shortname.patch` files. There
is no `series` file — CI applies the patches with `git apply` in `ls | sort` order, so the
numeric `NNN` prefix *is* the apply order.

Included patches — the **`00x`** range is generic upstream bug fixes; **`1xx`** is the
ARIB / ISDB feature patches:

- **`001-dvr-subscription-title-utf8.patch`** — fixes the web UI websocket dying once a
  recording exists. Tvheadend builds the recording's subscription title with
  `snprintf(buf, …, "DVR: %s", title)` into a fixed buffer; a long title (Japanese is
  3 bytes per character) gets truncated mid-UTF-8-character, and that invalid string is
  streamed in the comet status feed — the browser rejects the non-UTF-8 frame and closes the
  socket. The patch trims the dangling partial character with `utf8_validate_inplace()`.
  Not ARIB-specific — a generic Tvheadend buffer-truncation bug that long multibyte titles
  expose.
- **`002-isdb-s-linuxdvb-tune.patch`** — fixes ISDB-S tuning on Linux DVB adapters.
  Tvheadend already has ISDB-S support throughout (FE type, delivery system, network / mux /
  frontend classes, S2API tune commands), but the legacy parameter-copy `switch` in
  `linuxdvb_frontend_tune()` is missing the `DVB_TYPE_ISDB_S` case, so every ISDB-S tune hits
  the `default` arm, logs `unknown FE type 9`, and returns `SM_CODE_TUNING_FAILED` before the
  S2API path runs. The patch adds `case DVB_TYPE_ISDB_S` to the `ISDB_T`/`DAB` group — ISDB-S,
  like ISDB-T, carries no legacy `dvb_frontend_parameters` data and is tuned purely via the
  S2API. Not ARIB-specific — a generic upstream bug that blocks all ISDB-S reception.
- **`110-aribb24-text-encoding.patch`** — adds **ARIB STD-B24** (Japanese ISDB) text
  decoding via `libaribb24`, exposed as an `ARIB-STD-B24` option in the *Character set*
  dropdown (network / mux / service). When selected, every SI/EPG string from that tuner is
  decoded as ARIB STD-B24. Built in via `--enable-aribb24` (needs `libaribb24-dev`).
- **`120-aribb24-subtitle.patch`** — recognises ARIB STD-B24 caption (subtitle) elementary
  streams in ISDB PMTs (`stream_type 0x06` + ARIB data-component descriptor) as a new
  `ARIBSUB` component type, so their PID is filtered, passed through into recordings / TS
  streams, and shown in the web UI. Passthrough only — captions are carried, not rendered.
- **`130-aribb24-epg.patch`** — teaches the OTA EIT grabber the **ARIB STD-B10** EPG
  semantics used by ISDB. Descriptors: the content descriptor (`0x54`) genres are remapped
  from the ARIB genre table to the nearest DVB ETSI genre; the series descriptor (`0xD5`)
  supplies episode / total-episode numbers and a serieslink; the extended-event (`0x4E`)
  key/value detail items — which Tvheadend otherwise discards — are folded into the program
  description; the event group descriptor (`0xD6`) adds a short relay/shared-event note.
  It also fixes two ways ISDB EPG entries lost their text: a `short_event` decoding to blank
  (ASCII / U+3000 padding) no longer overwrites a real title, and a *partial* EIT pass — an
  EIT[schedule] "basic" segment or a shared/relayed event with no `short_event` — no longer
  makes `epg_broadcast_change_finish()` wipe the title/genre/description a fuller pass set
  (which made entries flicker between filled and empty). The ARIB descriptor paths activate
  only for services whose *Character set* is `ARIB-STD-B24`.
- **`150-isdb-cdt-logo.patch`** — extracts **ISDB station logos** from the CDT (Common Data
  Table, PID `0x29`) and uses them as channel icons. ISDB broadcasts each broadcaster's logo
  as a palettised PNG with the `PLTE`/`tRNS` chunks stripped; the patch parses the CDT,
  rebuilds the PNG from the fixed 129-entry ARIB logo CLUT, writes it under
  `<config>/isdb_logos/`, and sets it as the icon of every channel of the matching service
  (matched via the SDT `logo_transmission_descriptor`, tag `0xCF`). Terrestrial logos — BS/CS
  use a DSM-CC carousel (patch 151). A user-set channel icon is never overwritten.
- **`151-isdb-dsmcc-logo.patch`** — extracts **BS/CS** station logos, which (unlike
  terrestrial) ride a **DSM-CC data carousel** rather than the CDT. When a PMT advertises a
  data ES tagged `component_tag 0x79`/`0x7A` (the ARIB TR-B15 logo carousel), tvheadend opens
  a DSM-CC handler on it: a DII (`table_id 0x3B`) announces the `LOGO-05`/`CS_LOGO-05` module,
  DDB sections (`0x3C`) carry it block-by-block, and the reassembled module's per-service
  logos are reconstructed and applied exactly as for the CDT (patch 150). Implemented to the
  ARIB TR-B15 spec — it needs a transport that actually carries the carousel (a real ISDB-S
  tuner, or a full-transponder stream); service-filtered IPTV streams strip it.
- **`170-arib-channel-number.patch`** — populates ISDB channel numbers. The standard DVB
  logical-channel-number descriptors (0x83 etc.) are not used by ISDB; instead ARIB STD-B10
  carries the channel number ("リモコン番号") in the **TS Information descriptor** (tag
  `0xCD`) inside NIT, as a per-TS `remote_control_key_id` plus a list of `service_id`s. The
  patch parses that descriptor in the NIT TS-loop, walking the per-transmission-type service
  lists and assigning each service's `s_dvb_channel_num` from `remote_control_key_id` and
  `s_dvb_channel_minor` from the low 3 bits of `service_id` (the per-broadcaster service
  offset, per ARIB TR-B14), giving the familiar Japanese "X.Y" channel display (e.g. 4.1 for
  NTV primary, 4.2 for the sub-service). Without this, every ISDB service scanned by
  tvheadend shows channel number 0.0.

## Local patch development

The root `Makefile` is a **local-only** tool (CI does not use it). It needs `docker`,
`git`, `make`, and `quilt`. Run `make help` for all targets.

### Create or edit a patch

```sh
make clean prepare QUILT=1          # clone src/ + stage the quilt working series
export QUILTRC=$PWD/.quiltrc        # or: cp .quiltrc ~/.quiltrc   (one-time)
cd src
quilt push -a                       # apply the existing series
quilt new 010-shortname.patch       # start a new patch
quilt edit src/foo.c                # add + open a file (repeat per file)
quilt refresh                       # write the patch into src/patches/
cd ..
make build && make run              # compile + run to verify the new behaviour
make update                         # copy the .patch files back into patches/
git add patches/ && git commit
```

### Test build

`make build` is a fast, lightweight build (no transcoding, no DVB-scan, no `.deb`) producing
the runnable binary `src/build.linux/tvheadend`. `make run` launches it with the web UI on
<http://localhost:9981>. Object files persist under `src/build.linux/`, so repeat builds are
incremental.

## Notes

- arm64 build jobs use GitHub's `ubuntu-24.04-arm` runners — free for public repositories;
  a private repository needs a paid plan for arm64 runners.
- `src/`, `dist/` and `*.deb` are git-ignored build scratch / output.
