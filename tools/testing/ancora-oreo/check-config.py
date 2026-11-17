#!/usr/bin/env python3
"""Check the resolved Ancora config, not just requested defconfig symbols."""
import argparse
import pathlib
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('config', type=pathlib.Path)
args = parser.parse_args()
values = {}
for line in args.config.read_text().splitlines():
    if line.startswith('CONFIG_') and '=' in line:
        key, value = line.split('=', 1)
        values[key] = value
expected = {
    'CONFIG_ARM': 'y',
    'CONFIG_AEABI': 'y',
    'CONFIG_ANDROID_BINDER_IPC': 'y',
    'CONFIG_ANDROID_BINDER_IPC_32BIT': 'y',
    'CONFIG_ANDROID_BINDER_DEVICES': '"binder,hwbinder,vndbinder"',
    'CONFIG_SECCOMP': 'y',
    'CONFIG_SECCOMP_FILTER': 'y',
    'CONFIG_SECURITY': 'y',
    'CONFIG_SECURITY_SELINUX': 'y',
    'CONFIG_SECURITY_SELINUX_CHECKREQPROT_VALUE': '0',
    'CONFIG_AUDIT': 'y',
    'CONFIG_FHANDLE': 'y',
    'CONFIG_FUSE_FS': 'y',
    'CONFIG_TMPFS_XATTR': 'y',
    'CONFIG_EXT4_FS_SECURITY': 'y',
    'CONFIG_NETFILTER_XT_MATCH_QTAGUID': 'y',
    'CONFIG_CGROUPS': 'y',
    'CONFIG_IKCONFIG_PROC': 'y',
}
errors = [f'{key}: expected {value}, got {values.get(key, "unset")}'
          for key, value in expected.items() if values.get(key) != value]
if values.get('CONFIG_SECURITY_SELINUX_POLICYDB_VERSION_MAX') == 'y':
    errors.append('SELinux policy version must not be capped below 30')
for error in errors:
    print('FAIL:', error)
print(f'{len(expected)} resolved configuration requirements: '
      + ('FAIL' if errors else 'PASS'))
sys.exit(bool(errors))
