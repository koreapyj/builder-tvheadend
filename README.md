# builder-tvheadend

[tvheadend](https://tvheadend.org/) focused on Japanese DTV.

## Patches

`patches/` holds an OpenWrt-style quilt series of numbered `NNN-shortname.patch` files. There
is no `series` file — CI applies the patches with `git apply` in `ls | sort` order, so the
numeric `NNN` prefix *is* the apply order.

Patches follow a number convention by category:

- **`00x`** — generic upstream bug fixes (useful for every Tvheadend user, ARIB or not)
- **`1xx`** — ARIB / ISDB feature patches (Japanese broadcast support)
- **`2xx`** — additional device / tuner support

### Generic upstream bug fixes (`00x`)

- **`001-dvr-subscription-title-utf8.patch`** — Stops the Tvheadend web UI from freezing
  the moment a recording is in progress. Long recording titles containing CJK or other multibyte characters cause the live status feed to send a
  malformed UTF-8 frame; this patch keeps the frame valid so the UI keeps updating.

- **`002-eit-section-completion.patch`** — Makes the EPG grabber download the **full
  schedule** broadcasters transmit (often 7–8 days) instead of giving up after 60–120
  seconds with only 1–3 days. The grabber now waits until every EPG section the
  broadcaster says exists has actually arrived before releasing the tuner. Saves you
  from blank dates several days out in the programme guide.

### ARIB / ISDB feature patches (`1xx`)

These activate when you set the **Character set** of a Japanese network / mux / service
to `ARIB-STD-B24`. Otherwise they are inert and don't affect non-ARIB users.

- **`110-aribb24-text-encoding.patch`** — Adds **`ARIB-STD-B24`** to the *Character set*
  dropdown. Set it on your Japanese network and channel names, programme titles and
  descriptions display correctly in Japanese instead of as garbled bytes. Requires
  `libaribb24` (installed automatically by the build).

- **`120-aribb24-subtitle.patch`** — Recognises Japanese closed-caption (字幕) streams
  so they're carried through into recordings and live streams. Captions are passed
  through, not rendered — pick them up in your downstream player.

- **`130-aribb24-epg.patch`** — Makes the **programme guide** for Japanese channels look
  right. Genre, episode numbers, the extended description fields (cast, director, etc.),
  and programme-relay / shared-event notes — all of which vanilla Tvheadend throws away —
  now show up. Also stops EPG entries from flickering between filled and blank as
  successive EPG passes arrive.

- **`140-isdb-s-broadcast-support.patch`** — Makes Japanese **BS / 110°E CS satellite**
  actually tunable. Without this, every ISDB-S tune attempt fails with "tuning failed",
  imported BS/CS scanfiles silently don't load, and mux frequencies display incorrectly
  in the UI. Required for any BS/CS reception.

- **`150-isdb-cdt-logo.patch`** — Automatically fetches Japanese **terrestrial (地デジ)**
  channel logos that the broadcaster transmits in the signal and applies them as channel
  icons. Logos appear after the channel has been tuned for a short while. A logo you've
  set manually is never overwritten.

- **`151-isdb-dsmcc-logo.patch`** — Same as 150 but for **BS / CS satellite** channels,
  which transmit their logos via a different mechanism. Needs a real tuner — IPTV streams
  strip the logo data out.

- **`152-isdb-logo-id-zero.patch`** — Fixes **NHK 総合** (NHK General) terrestrial
  channels failing to pick up their logo even after waiting all day. NHK's logo
  identifier happens to be `0`, which the earlier logo patches mistakenly treated as
  "no logo". With this fix NHK G's logo applies the moment a fresh SDT comes through.

- **`160-jp-isdb-scanfiles.patch`** — Bundles ready-to-import scanfiles for **Japan**
  ISDB-T (terrestrial UHF channels 13–62) and ISDB-S (BS / 110°E CS, all 12 TLV slots).
  Pick `isdb-t/jp-Japan` or `isdb-s/jp-Japan` from Tvheadend's scan-network dropdown
  instead of typing muxes by hand.

- **`170-arib-channel-number.patch`** — Gives Japanese channels their familiar **X.Y
  channel numbers** (e.g. `4.1` for NTV's main service, `4.2` for the sub-channel).
  Without this, every ISDB channel in Tvheadend shows up as channel `0.0`.

- **`180-aribb25-descrambler.patch`** — **Decrypts encrypted Japanese broadcasts**
  (B-CAS / MULTI2). Two card backends to pick from:
  - **[CobaltCas](https://github.com/CobaltCas/CobaltCas)** — software Blue Card emulator. Works out of the box, no physical card
    needed; ideal for unencrypted-Blue-Card-tier content.
  - **PC/SC** — a real B-CAS card (typically a Red Card) plugged into a PC/SC reader,
    for pay-channel entitlements.

  Configured under *Configuration → CAs* per descrambler instance; activates automatically
  on any service that needs it.

### Device / tuner support (`2xx`)

- **`200-pt1-device-support.patch`** — Adds tuner support for **Earthsoft PT1 / PT3**
  PCI cards and the **PLEX / Digibest USB tuners** commonly used in Japan: PX5,
  PX-MLT5PE, PX-MLT8PE, Digibest ISDB6014, plus the Asicen ASV5220. Works through the
  [nns779/px4_drv](https://github.com/nns779/px4_drv) Linux kernel driver (build separately
  per the driver's instructions). Each tuner appears as a configurable adapter in the
  WebUI under *Configuration → DVB Inputs → TV adapters*.

  If you wish to use hardware PID filter, use modified driver [koreapyj/px4_drv](https://github.com/koreapyj/px4_drv).

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

## Special Thanks

This project is highly inspired by Mirakurun project.
