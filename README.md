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
