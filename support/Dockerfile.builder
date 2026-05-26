ARG BASE=ubuntu:noble
FROM ${BASE}

ARG BUILD_DEPS="build-essential cmake git pkg-config gettext \
    libavahi-client-dev libssl-dev zlib1g-dev liburiparser-dev \
    libpcre2-dev libdvbcsa-dev libaribb24-dev libpcsclite-dev \
    libpcsclite1 python3 wget bzip2 ca-certificates"
ARG LIBARIBB25_REPO=https://github.com/koreapyj/libaribb25.git
ARG LIBARIBB25_REF=master
ARG COBALTCAS_REF=master

ENV DEBIAN_FRONTEND=noninteractive

# Apt deps. BuildKit cache mounts keep iteration on this Dockerfile fast.
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update -y && \
    apt-get install -y --no-install-recommends ${BUILD_DEPS} && \
    rm -rf /var/lib/apt/lists/*

# libaribb25 + CobaltCas baked into the image.
COPY build-libaribb25.sh /tmp/build-libaribb25.sh
RUN LIBARIBB25_REPO="${LIBARIBB25_REPO}" \
    LIBARIBB25_REF="${LIBARIBB25_REF}" \
    COBALTCAS_REF="${COBALTCAS_REF}" \
    PREFIX=/usr \
    bash /tmp/build-libaribb25.sh && \
    rm /tmp/build-libaribb25.sh

# Avoid "git: detected dubious ownership" on the bind-mounted /ws.
RUN git config --system --add safe.directory '*'

WORKDIR /ws
