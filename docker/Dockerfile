# Build environment for the STM32MP2's Linux side (Cortex-A35, 64-bit ARM).
# Runs as an arm64 Debian container (emulated on the Intel Mac), so g++ builds
# programs for the board directly. Use -static so the program does not depend on
# the board's library versions.
FROM --platform=linux/arm64 debian:bookworm
RUN apt-get update \
 && apt-get install -y --no-install-recommends g++ make \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
