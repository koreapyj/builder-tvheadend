#!/bin/sh
# Package the built tvhepg-extract binary as a standalone Debian package.
# Usage: mkdeb.sh [OUTDIR] [VERSION] [ARCH]
#   OUTDIR  : output dir (and where tvheadend_*.deb is found for ver/arch) (default: ..)
#   VERSION : package version (default: read from tvheadend_*.deb in OUTDIR)
#   ARCH    : dpkg arch     (default: read from tvheadend_*.deb in OUTDIR, else dpkg --print-architecture)
# Run AFTER `make -C epgextract`.
set -eu

ROOT="$(CDPATH= cd "$(dirname "$0")" && pwd)"
OUTDIR="${1:-$ROOT/..}"
VER="${2:-}"
ARCH="${3:-}"

if [ -z "$VER" ] || [ -z "$ARCH" ]; then
  TVHDEB="$(ls "$OUTDIR"/tvheadend_*.deb 2>/dev/null | head -1 || true)"
  if [ -n "$TVHDEB" ]; then
    [ -n "$VER" ]  || VER="$(dpkg-deb -f "$TVHDEB" Version)"
    [ -n "$ARCH" ] || ARCH="$(dpkg-deb -f "$TVHDEB" Architecture)"
  fi
fi
[ -n "$VER" ]  || { echo "mkdeb: no VERSION (and no tvheadend_*.deb in $OUTDIR)" >&2; exit 1; }
[ -n "$ARCH" ] || ARCH="$(dpkg --print-architecture)"
[ -x "$ROOT/tvhepg-extract" ] || { echo "mkdeb: $ROOT/tvhepg-extract not built" >&2; exit 1; }

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
install -D -m0755 "$ROOT/tvhepg-extract" "$STAGE/usr/bin/tvhepg-extract"

# Compute the real shared-library dependencies (libaribb24, libssl/crypto,
# libpcre2, libpng, zlib, libc, ...) from the binary itself — NOT a dependency
# on the tvheadend daemon package. Fall back to "no deps" if shlibdeps is absent.
DEPS=""
if command -v dpkg-shlibdeps >/dev/null 2>&1; then
  ( cd "$STAGE"
    mkdir -p debian
    printf 'Source: tvheadend-epgextract\n\nPackage: tvheadend-epgextract\nArchitecture: any\n' > debian/control
    dpkg-shlibdeps -O --ignore-missing-info usr/bin/tvhepg-extract 2>/dev/null
  ) > "$STAGE/.deps" || true
  DEPS="$(sed -n 's/^shlibs:Depends=//p' "$STAGE/.deps")"
  rm -rf "$STAGE/debian" "$STAGE/.deps"
fi

mkdir -p "$STAGE/DEBIAN"
{
  echo "Package: tvheadend-epgextract"
  echo "Version: $VER"
  echo "Architecture: $ARCH"
  echo "Maintainer: builder-tvheadend"
  echo "Section: video"
  echo "Priority: optional"
  [ -n "$DEPS" ] && echo "Depends: $DEPS"
  echo "Description: Standalone ARIB EPG extractor for MPEG-TS files"
  echo " Reuses tvheadend's EIT parser (with this fork's ARIB patches) to extract"
  echo " the currently-running programme (--now, default) or the full schedule"
  echo " (--all) from an MPEG-TS recording, emitted as JSON. Self-contained: depends"
  echo " only on shared libraries, not on the tvheadend daemon."
} > "$STAGE/DEBIAN/control"

OUT="$OUTDIR/tvheadend-epgextract_${VER}_${ARCH}.deb"
dpkg-deb --build --root-owner-group "$STAGE" "$OUT"
echo "mkdeb: built $OUT"
echo "mkdeb: Depends: ${DEPS:-<none>}"
