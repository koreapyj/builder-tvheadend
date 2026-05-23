#!/usr/bin/env bash
#
# Build libaribb25 with CobaltCas and install it into the build environment.
# Run inside the Debian/Ubuntu build container that compiles tvheadend.
#
# Required APT packages (caller installs): build-essential cmake git
#                                          libpcsclite-dev pkg-config
#
# Env vars:
#   LIBARIBB25_REPO   git URL          (default: https://github.com/koreapyj/libaribb25.git)
#   LIBARIBB25_REF    git ref/tag      (default: master)
#   COBALTCAS_REF     CobaltCas ref    (default: v1.0.0)
#   PREFIX            install prefix   (default: /usr/local)

set -euxo pipefail

LIBARIBB25_REPO="${LIBARIBB25_REPO:-https://github.com/koreapyj/libaribb25.git}"
LIBARIBB25_REF="${LIBARIBB25_REF:-master}"
COBALTCAS_REF="${COBALTCAS_REF:-master}"
PREFIX="${PREFIX:-/usr/local}"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

git clone --depth=1 -b "$LIBARIBB25_REF" "$LIBARIBB25_REPO" "$WORK/src"
cd "$WORK/src"

# Pin CobaltCas to a known commit/tag instead of master, for reproducibility.
sed -i "s|GIT_TAG master|GIT_TAG ${COBALTCAS_REF}|" CMakeLists.txt

# Inject runtime setters that tvheadend uses to point CobaltCas at a per-user
# card-image path / log file / log mode (the upstream b_cas_cobalt.cpp exposes
# only the create function, with `sys` as a translation-unit-private global).
cat >> aribb25/b_cas_cobalt.cpp <<'COBALT_SETTERS_EOF'

extern "C" void b_cas_cobalt_set_card_image_path(const char *path)
{
	if (path && *path)
		sys.CARD_IMAGE_FILE_NAME = path;
}

extern "C" void b_cas_cobalt_set_log_file_path(const char *path)
{
	if (path && *path)
		sys.LOG_FILE_NAME = path;
}

extern "C" void b_cas_cobalt_set_log_mode(int mode)
{
	sys.logMode = (uint16_t)mode;
}
COBALT_SETTERS_EOF

sed -i '/^extern B_CAS_CARD \*create_b_cas_cobalt/a\
extern void b_cas_cobalt_set_card_image_path(const char *path);\
extern void b_cas_cobalt_set_log_file_path(const char *path);\
extern void b_cas_cobalt_set_log_mode(int mode);' aribb25/b_cas_cobalt.h

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DUSE_COBALTCAS=ON
cmake --build build -j"$(nproc)"
cmake --install build

# libaribb25's CMakeLists only builds the static archive (the shared target is
# commented out), but its install rules drop a dangling `lib*arib25.so` symlink
# to a non-existent `lib*aribb25.so`. Repair: install the real .a archives and
# remove the dead symlinks. Then patch the installed pkg-config file to declare
# the private deps that the static archive needs (libpcsclite for the PCSC card
# backend, libstdc++ for the CobaltCas C++ code).
#
# Use the libdir that CMake actually installed to (multiarch-aware: /usr/lib on
# /usr/local, /usr/lib/<triple> on /usr).
LIBDIR=$(pkg-config --variable=libdir libaribb25)

install -m 644 build/libaribb25.a "$LIBDIR/libaribb25.a"
install -m 644 build/libaribb1.a  "$LIBDIR/libaribb1.a"
rm -f "$LIBDIR/libarib25.so" "$LIBDIR/libarib1.so"

sed -i 's|^Libs: \(.*-laribb25\)$|Libs: \1 -lpcsclite -lstdc++|' \
  "$LIBDIR/pkgconfig/libaribb25.pc"

ldconfig 2>/dev/null || true

echo "libaribb25 installed under $PREFIX"
pkg-config --modversion libaribb25 || true
pkg-config --libs libaribb25 || true
