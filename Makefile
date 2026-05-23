# Local-only patch-authoring and test-build workflow for the Tvheadend builder.
# GitHub Actions does NOT use this Makefile — see .github/workflows/build.yaml.

TVH_REPO    ?= https://github.com/tvheadend/tvheadend.git
TVH_REF     ?= master
BUILD_IMAGE ?= ubuntu:noble
QUILT       ?=

SRC     := src
PATCHES := patches
QUILTRC := $(CURDIR)/.quiltrc

# Minimal dependency set — enough to compile a runnable tvheadend, nothing for
# transcoding or Debian packaging.
BUILD_DEPS := build-essential cmake git pkg-config gettext libavahi-client-dev \
              libssl-dev zlib1g-dev liburiparser-dev libpcre2-dev libdvbcsa-dev \
              libaribb24-dev libpcsclite-dev python3 wget bzip2 ca-certificates

# Minimal configure flags — fast, offline, no transcoding / DVB-scan fetch.
# --prefix=/usr puts TVHEADEND_DATADIR at /usr/share/tvheadend, matching the
# Debian package layout (so `make -C src install` lands where dpkg would).
# ARIB STD-B24 text decoding is kept on so patches to it can be tested locally.
BUILD_CONFIGURE := --prefix=/usr \
                   --disable-ffmpeg_static --disable-hdhomerun_static \
                   --disable-libav --disable-dvbscan --enable-aribb24 \
                   --python=python3

# Runtime shared libraries needed to *run* the test binary. The -dev package
# names are stable across releases and pull in the correct SONAME packages.
RUN_DEPS := libavahi-client-dev libssl-dev zlib1g-dev liburiparser-dev \
            libpcre2-dev libdvbcsa-dev libaribb24-dev libpcsclite1

.DEFAULT_GOAL := help
.PHONY: help prepare build run update refresh clean

help:
	@echo 'Local patch-authoring / test-build workflow (not used by CI).'
	@echo
	@echo 'Targets:'
	@echo '  prepare [QUILT=1]  clone tvheadend into $(SRC)/; default applies the patch'
	@echo '                     series with git apply, QUILT=1 stages a quilt working series'
	@echo '  build              lightweight test build -> $(SRC)/build.linux/tvheadend'
	@echo '  run                run the freshly built binary (web UI on http://localhost:9981)'
	@echo '  update             copy the edited series from $(SRC)/$(PATCHES)/ back to $(PATCHES)/'
	@echo '  refresh            rebase the whole series against current source, then update'
	@echo '  clean              remove $(SRC)/'
	@echo
	@echo 'Variables: TVH_REF (default master), TVH_REPO, BUILD_IMAGE (default ubuntu:noble)'
	@echo
	@echo 'Patch workflow:  make clean prepare QUILT=1  ->  edit with quilt  ->  make update'
	@echo 'Test build:      make clean prepare build  ->  make run'

$(SRC)/.git:
	git clone $(TVH_REPO) $(SRC)
	git -C $(SRC) checkout $(TVH_REF)

prepare: $(SRC)/.git
ifeq ($(QUILT),1)
	@mkdir -p $(SRC)/$(PATCHES)
	@if ls $(PATCHES)/*.patch >/dev/null 2>&1; then cp $(PATCHES)/*.patch $(SRC)/$(PATCHES)/; fi
	@# quilt needs a series file; generate it fresh from the patch filenames
	@( cd $(SRC)/$(PATCHES) && ls *.patch 2>/dev/null | sort > series || : )
	@echo
	@echo 'Quilt working series staged in $(SRC)/. Next:'
	@echo '  export QUILTRC=$(QUILTRC)   # or: cp .quiltrc ~/.quiltrc'
	@echo '  cd $(SRC) && quilt push -a'
	@echo '  quilt new NNN-shortname.patch ; quilt edit <file> ; quilt refresh'
	@echo '  cd .. && make update'
else
	@set -e; \
	for p in $$(cd $(PATCHES) && ls *.patch | sort); do \
		echo "Applying $(PATCHES)/$$p"; \
		git -C $(SRC) apply "$(CURDIR)/$(PATCHES)/$$p"; \
	done
endif

build:
	@test -d $(SRC) || { echo "No $(SRC)/ — run 'make prepare' (or 'make prepare QUILT=1') first"; exit 1; }
	docker run --rm -e DEBIAN_FRONTEND=noninteractive \
		-v "$(CURDIR):/ws" -w /ws/$(SRC) "$(BUILD_IMAGE)" \
		bash -euxc 'apt-get update -y && apt-get install -y $(BUILD_DEPS) && PREFIX=/usr bash /ws/support/build-libaribb25.sh && git config --global --add safe.directory "*" && ./configure $(BUILD_CONFIGURE) && make -j"$$(nproc)" && chown -R "$$(stat -c %u:%g /ws)" .'
	@echo
	@echo 'Built: $(SRC)/build.linux/tvheadend'

run:
	@test -x $(SRC)/build.linux/tvheadend || { echo "Not built — run 'make build' first"; exit 1; }
	docker run --rm -it -p 9981:9981 -e DEBIAN_FRONTEND=noninteractive \
		-v "$(CURDIR):/ws" -w /ws/$(SRC) "$(BUILD_IMAGE)" \
		bash -c 'apt-get update -y >/dev/null && apt-get install -y $(RUN_DEPS) >/dev/null && exec ./build.linux/tvheadend -c /tmp/tvheadend -C'

update:
	@test -d $(SRC)/$(PATCHES) || { echo "No $(SRC)/$(PATCHES)/ — run 'make prepare QUILT=1' first"; exit 1; }
	@mkdir -p $(PATCHES)
	@rm -f $(PATCHES)/*.patch
	@if ls $(SRC)/$(PATCHES)/*.patch >/dev/null 2>&1; then cp $(SRC)/$(PATCHES)/*.patch $(PATCHES)/; fi
	@echo "Patches copied back to $(PATCHES)/ — review and commit."

refresh:
	@test -d $(SRC)/$(PATCHES) || { echo "Run 'make prepare QUILT=1' first"; exit 1; }
	cd $(SRC) && QUILTRC=$(QUILTRC) sh -c 'quilt pop -a >/dev/null 2>&1 || true; while quilt push; do quilt refresh; done'
	@$(MAKE) update

clean:
	rm -rf $(SRC)
