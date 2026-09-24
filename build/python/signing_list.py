#!/usr/bin/env python3
"""List the binaries the Windows installer ships, for Authenticode signing.

The release build signs every executable and library the MSI installs, and
nothing else: the build tree also holds the unit-test executables (over a
hundred of them, next to nscp.exe), and each signature is billed. So the list
is taken from the installer itself, the File elements of its .wxs files, rather
than from a folder listing.

A File's Source carries WiX preprocessor variables ($(var.Source)/nscp.exe,
$(var.Source)/$(var.OpenSSLCryptoDll)). They are resolved from wix-defines.txt,
which installers/installer-NSCP/CMakeLists.txt writes from the same -d flags it
hands to candle, so the names here are the names the MSI packs.

Printed: one absolute path per line, sorted, for every binary that exists.
A File whose variable is not defined (the OpenSSL DLLs of a static build) or
whose file is absent (Python on ARM64, where the .wxs leaves it out behind an
<?if?>) is reported on stderr and skipped: the MSI build, not this list, is
what fails when a file it needs is missing.

    python build/python/signing_list.py \
        --wix-dir installers/installer-NSCP \
        --defines tmp/nscp/installers/installer-NSCP/wix-defines.txt
"""

import argparse
import glob
import os
import re
import sys

SIGNABLE = ('.exe', '.dll', '.pyd')

FILE_SOURCE = re.compile(r'<File\b[^>]*?\bSource="([^"]+)"', re.S)
VARIABLE = re.compile(r'\$\(var\.([^)]+)\)')
COMMENT = re.compile(r'<!--.*?-->', re.S)


class UndefinedVariable(Exception):
    pass


def read_defines(path):
    defines = {}
    with open(path, encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\r\n')
            if not line or '=' not in line:
                continue
            name, value = line.split('=', 1)
            defines[name] = value
    return defines


def sources(wix_dir):
    """Every File Source in the .wxs files of wix_dir, in file order."""
    for wxs in sorted(glob.glob(os.path.join(wix_dir, '*.wxs'))):
        with open(wxs, encoding='utf-8') as f:
            text = COMMENT.sub('', f.read())
        for match in FILE_SOURCE.finditer(text):
            yield wxs, match.group(1)


def resolve(source, defines):
    def value(match):
        name = match.group(1)
        if name not in defines:
            raise UndefinedVariable(name)
        return defines[name]
    return VARIABLE.sub(value, source)


def signable_files(wix_dir, defines, log=None):
    log = log or sys.stderr
    found = set()
    for wxs, source in sources(wix_dir):
        try:
            path = resolve(source, defines)
        except UndefinedVariable as e:
            # Only worth a line when the name could be a binary: an undefined
            # variable in a help or web path is not this list's business.
            if source.lower().endswith(SIGNABLE) or '$(var.' in os.path.basename(source):
                print('skipped %s (%s): $(var.%s) is not defined' % (source, os.path.basename(wxs), e), file=log)
            continue
        if not path.lower().endswith(SIGNABLE):
            continue
        path = os.path.abspath(os.path.normpath(path.replace('\\', '/')))
        if not os.path.isfile(path):
            print('skipped %s: not built' % path, file=log)
            continue
        found.add(path)
    return sorted(found)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n', 1)[0])
    parser.add_argument('--wix-dir', required=True, help='folder with the installer .wxs files')
    parser.add_argument('--defines', required=True, help='wix-defines.txt written by the installer CMakeLists.txt')
    args = parser.parse_args(argv)

    files = signable_files(args.wix_dir, read_defines(args.defines))
    if not files:
        print('error: no signable file found; is the build tree at the Source in %s?' % args.defines, file=sys.stderr)
        return 1
    for path in files:
        print(path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
