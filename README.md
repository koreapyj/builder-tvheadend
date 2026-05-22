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

`patches/` holds an OpenWrt-style quilt series: numbered `NNN-shortname.patch` files plus a
`series` file. CI applies the series with `git apply`.

Included patches:

- **`010-aribb24-text-encoding.patch`** — adds **ARIB STD-B24** (Japanese ISDB) text
  decoding via `libaribb24`, exposed as an `ARIB-STD-B24` option in the *Character set*
  dropdown (network / mux / service). When selected, every SI/EPG string from that tuner is
  decoded as ARIB STD-B24. Built in via `--enable-aribb24` (needs `libaribb24-dev`).
- **`020-aribb24-subtitle.patch`** — recognises ARIB STD-B24 caption (subtitle) elementary
  streams in ISDB PMTs (`stream_type 0x06` + ARIB data-component descriptor) as a new
  `ARIBSUB` component type, so their PID is filtered, passed through into recordings / TS
  streams, and shown in the web UI. Passthrough only — captions are carried, not rendered.
- **`030-aribb24-epg.patch`** — teaches the OTA EIT grabber the **ARIB STD-B10** EPG
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
- **`040-dvr-subscription-title-utf8.patch`** — fixes the web UI websocket dying once a
  recording exists. Tvheadend builds the recording's subscription title with
  `snprintf(buf, …, "DVR: %s", title)` into a fixed buffer; a long title (Japanese is
  3 bytes per character) gets truncated mid-UTF-8-character, and that invalid string is
  streamed in the comet status feed — the browser rejects the non-UTF-8 frame and closes the
  socket. The patch trims the dangling partial character with `utf8_validate_inplace()`.
  Not ARIB-specific — a generic Tvheadend buffer-truncation bug that long multibyte titles
  expose.

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
make update                         # copy the series back into patches/
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
