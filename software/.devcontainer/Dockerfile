FROM luckfoxtech/luckfox_pico:1.0

RUN apt-get update && apt-get install -y --no-install-recommends \
    bash-completion \
    gdb-multiarch \
    curl \
    tar \
    coreutils \
    adb \
    ca-certificates \
    clangd \
    && rm -rf /var/lib/apt/lists/* \
    && chmod 1777 /tmp

# ── 1. Luckfox SDK toolchain (cross-compiler only, sparse checkout) ─
RUN git clone --depth 1 --filter=blob:none --sparse \
        https://github.com/LuckfoxTECH/luckfox-pico.git /opt/luckfox-pico \
    && cd /opt/luckfox-pico \
    && git sparse-checkout set \
        tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf

# ── 2. RKMPI headers + prebuilt uclibc libraries ─────────────────────
# Clone only include/ and lib/uclibc/ from the rkmpi_example repo;
# this avoids pulling gigabytes of the full SDK build tree.
RUN git clone --depth 1 --filter=blob:none --sparse \
        --branch kernel-5.10.160 \
        https://github.com/LuckfoxTECH/luckfox_pico_rkmpi_example.git \
        /opt/luckfox-rkmpi \
    && cd /opt/luckfox-rkmpi \
    && git sparse-checkout set include lib/uclibc

# Add toolchain to PATH and bake SDK/RKMPI paths as env variables so
# build.sh and any IDE tooling can find them without a manual export.
ENV PATH="/opt/luckfox-pico/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin:${PATH}"
ENV LUCKFOX_SDK_PATH="/opt/luckfox-pico"
ENV RKMPI_ROOT="/opt/luckfox-rkmpi"
ENV ANDROID_ADB_SERVER_ADDRESS="host.docker.internal"

WORKDIR /workspaces/rv1103-dev

# Install the Antigravity CLI for agentic autonomy
RUN curl -fsSL https://antigravity.google/install.sh | sh -s -- --version 2.4.1

# Ensure the agent has permission to use the serial/USB ports for\ the Luckfox
RUN usermod -aG dialout,video root

# Force the HOME directory so the installer doesn't get lost
ENV HOME=/root
WORKDIR /root