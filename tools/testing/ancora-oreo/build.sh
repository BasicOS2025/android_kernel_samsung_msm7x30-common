#!/usr/bin/env bash
set -euo pipefail

kernel_root=$(cd -- "$(dirname -- "$0")/../../.." && pwd)
kernel_out=${KERNEL_OUT:-"$kernel_root/out-ancora"}
kernel_cross=${CROSS_COMPILE:?Set CROSS_COMPILE to an ARM GCC 4.8 prefix, including the trailing hyphen}
kernel_jobs=${JOBS:-$(getconf _NPROCESSORS_ONLN)}
command -v "${kernel_cross}gcc" >/dev/null
mkdir -p -- "$kernel_out"
kernel_out=$(cd -- "$kernel_out" && pwd)
cd -- "$kernel_root"

make_args=("O=$kernel_out" ARCH=arm "CROSS_COMPILE=$kernel_cross" "HOSTCFLAGS=-O2 -fcommon")
make "${make_args[@]}" ancora_defconfig
python3 tools/testing/ancora-oreo/check-config.py "$kernel_out/.config"
make -j"$kernel_jobs" "${make_args[@]}" zImage modules
make "${make_args[@]}" headers_install
"${kernel_cross}gcc" -marm -O2 -Wall -Wextra -Werror \
    -fomit-frame-pointer -fno-stack-protector -fno-builtin -nostdlib -static \
    -Wl,-e,_start -I"$kernel_out/usr/include" -Idrivers/staging/android \
    tools/testing/ancora-oreo/smoke.c -o "$kernel_out/ancora-oreo-smoke"
printf 'Kernel: %s\nDevice smoke test (not executed): %s\n' \
    "$kernel_out/arch/arm/boot/zImage" "$kernel_out/ancora-oreo-smoke"
