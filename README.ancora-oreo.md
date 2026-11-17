# Ancora / GT-I8150 — Android 8.1 kernel port

This branch adapts the requested `arco/samsung-kernel-msm7x30` CM12 kernel
for the ARM32 LineageOS 15.1 device tree. It remains Linux **3.4.107**;
the kernel version has not been relabeled as a newer upstream release.

**Status: cross-compiled development port; no physical-device boot or hardware
test has been performed.** A successful kernel build does not establish that
the whole Android 8.1 ROM boots. The separate camera, graphics, audio, media and
radio userspace ports are still required.

## Source provenance

- Requested base: https://github.com/arco/samsung-kernel-msm7x30,
  `cm-12.0`, commit `b157ddfd3a28f6e6b2dca4acfc8fcbd0ad684489`.
- Reference for the 3.4 backports:
  https://github.com/LineageOS/android_kernel_samsung_msm8930-common,
  `lineage-15.1`, commit `450bc883eabe96426af15d032ddda86572fc319c`.
- Compiler: AOSP `platform/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8`,
  `lollipop-release`, commit `26e93f6af47f7bd3a9beb5c102a5f45e19bfa38a`.

Only the required generic IPC/security interfaces were brought over. Ancora's
board, clock, modem, display and other hardware implementation remains from
the requested source. Donor platform configuration and root-management code
were not imported. Existing copyright/license notices remain in the sources.

## Implemented changes

- Binder per-device contexts for `binder,hwbinder,vndbinder`, scatter/gather
  transactions and buffer/FD-array objects used by HIDL. The existing 32-bit,
  protocol-v7 ABI is selected to match `TARGET_USES_64_BIT_BINDER := false`
  in the prepared device tree. Do not enable the 64-bit Binder userspace option
  against this configuration.
- Binder's file-descriptor helpers retain this tree's 3.4 file-table APIs;
  the donor's newer `__alloc_fd`/`__fd_install`/`__close_fd` APIs were not
  assumed to exist. Lightweight UID helpers match the existing non-userns ABI.
- Seccomp BPF filters, `seccomp(2)` at ARM EABI syscall 383, TSYNC support,
  filter reference management and inheritance under the sighand lock.
- ARM syscall argument access and filter entry hooks while preserving Ancora's
  existing TLS/restart conventions. The skipped-syscall path reloads the saved
  result instead of sending syscall `-1` through the private ARM SWI handler.
- `PR_SET_NO_NEW_PRIVS`/`PR_GET_NO_NEW_PRIVS`, an independent atomic flag at
  bit index 0, inheritance, setuid/setgid suppression on exec, and capability/LSM
  checks. The flag does not overlap cpuset's existing bit indexes 1 and 2.
- Ambient capability operations and exec credential handling needed by
  Android init services with `capabilities` declarations.
- SELinux policy versions through 30, including constraint names, object
  defaults and ioctl extended permissions. Genfs labeling is available for
  sysfs/debugfs/pstore. The existing LSM mount hook signature is retained.
- SIGSYS syscall/architecture/address information copied through signal and
  signalfd interfaces; signalfd's public record size remains 128 bytes.
- `getrandom(2)` at ARM EABI syscall 384, with initialization waiting,
  nonblocking/error semantics and public flags. Existing entropy accounting
  is retained rather than replacing the RNG with an unrelated subsystem.
- BCM4329 MAC handling validates each octet without wide `sscanf` writes into
  a six-byte array. Existing valid addresses are retained; missing or malformed
  files get a locally administered unicast address. Formatting reserves space
  for the newline and terminator, and file-read failures are checked.
- `ancora_defconfig` enables the new interfaces, file handles and config
  introspection, and sets SELinux `checkreqprot` to 0. Enforcing checks are
  not bypassed. The existing SELinux development-mode configuration remains.

## Build

Place this repository at `kernel/samsung/msm7x30-common` in a complete
LineageOS 15.1 tree. The matching product expects `ancora_defconfig`.
An example repo local manifest for the three companion trees is provided in
`Documentation/ancora-oreo-local-manifest.xml`. Use it only when those paths
are not already defined by another manifest; it does not provide the missing
legacy userspace HAL dependencies.

For a standalone build with an ARM GCC 4.8 toolchain:

```sh
export CROSS_COMPILE=/absolute/path/to/arm-eabi-4.8/bin/arm-eabi-
export KERNEL_OUT=/absolute/path/to/kernel-output
export JOBS=3
bash tools/testing/ancora-oreo/build.sh
```

Host requirements include GCC, make, Python 3, Perl, xz and standard Linux
build utilities. The script uses `HOSTCFLAGS=-O2 -fcommon` so the old host
utilities also build with GCC 10 and later. Target code is compiled with
GCC 4.8. A modern ARM GCC/Clang toolchain has not been validated for this tree.

The script builds `zImage` and modules, installs userspace headers and compiles
the static ARM smoke-test program. It checks the **resolved** `.config`, not
just the text in `ancora_defconfig`. Build results are recorded separately in
`Documentation/ancora-oreo-build.txt`.
The additional Wi-Fi MAC regression build is recorded in
`Documentation/ancora-wifi-mac-build.txt`.

The MAC parser regression test can also run on the build host:

```sh
cc -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
  tools/testing/ancora-oreo/mac-test.c -o /tmp/ancora-mac-test
/tmp/ancora-mac-test
```

This checks valid saved addresses, high-bit octets, malformed/multicast/zero
addresses, destination guard bytes and 100,000 bounded inputs. It does not test
Wi-Fi firmware loading or association on the actual phone.

## On-device checks still required

The smoke program can be run after integrating the kernel into a compatible
Android boot image and booting it on the target device:

```sh
adb push "$KERNEL_OUT/ancora-oreo-smoke" /data/local/tmp/
adb shell chmod 755 /data/local/tmp/ancora-oreo-smoke
adb shell /data/local/tmp/ancora-oreo-smoke
```

It checks protocol 7 on all three Binder nodes without taking over any context
manager; SELinux policy version 30; nonblocking getrandom; ambient-clear/get;
no_new_privs; rejection of unknown seccomp flags; acceptance of the TSYNC flag;
ERRNO result preservation and filter inheritance through fork. It also checks
that the parent process is unaffected by the child's filter. It does **not**
test synchronization of multiple live threads, Binder SG transfers, complete
SELinux policy loading, setuid/file-capability exec cases, or hardware drivers.
An access-denied result on a device node must be investigated in permissions
and AVC logs; it is not a successful Binder test.

The runtime test has been compiled, **not executed against this kernel**.
Further tests must cover multi-thread TSYNC, signal traps, Binder contexts and
FD transfers under load, policy loading, suspend/resume, storage, modem, camera,
graphics and BCM4329 firmware/module behavior.

## Integration limits

- This is a legacy, non-Treble upgrade configuration. Three Binder nodes do not
  make the device Treble compliant or establish CTS/VTS compatibility.
- No boot/recovery image or flash installer is produced here. The raw `zImage`
  is only the kernel payload. The Android boot header, base/pagesize, ramdisk,
  and the 5,767,168-byte boot partition limit belong to the device build.
- Loadable modules must come from the same kernel build. `dhd.ko` belongs at
  `/system/lib/modules/dhd.ko` for the prepared device tree. BCM4329 Wi-Fi
  firmware must be supplied separately.
- The source does not receive every stable/security update from the donor,
  and its version stays 3.4.107. Network socket-destroy backports and newer
  namespace/sdcardfs features were not imported. Oreo netd behavior must be
  checked on-device; the prepared userspace selects FUSE rather than sdcardfs.
- `getrandom` may return EAGAIN with GRND_NONBLOCK, or block without it, until
  the original kernel entropy pool initializes. Early-boot entropy must be
  checked on the actual board.

Companion sources:

- https://github.com/BasicOS2025/android_device_samsung_ancora/tree/lineage-15.1
- https://github.com/BasicOS2025/android_vendor_samsung_ancora/tree/lineage-15.1
