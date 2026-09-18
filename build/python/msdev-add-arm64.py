"""Add ARM64 configurations to a Visual Studio project that only ships x86/x64.

Crypto++ 8.9.0 (and every release before it) declares exactly four platform
combinations in cryptlib.vcxproj -- Debug/Release x Win32/x64 -- so
`msbuild /p:Platform=ARM64` fails with MSB4126 ("the specified solution
configuration is invalid") before a single file is compiled. The sources
themselves are fine on ARM64: config_asm.h keys off _M_ARM64 and the hand
written assembly (rdrand.asm, rdseed.asm, x64masm.asm, ...) is attached to
CustomBuild items that are already conditioned on '$(Platform)'=='Win32' or
'x64', so an ARM64 platform simply skips them.

That makes the missing configurations pure boilerplate, which is what this
script fills in: every ProjectConfiguration and every platform-conditioned
ItemDefinitionGroup that exists for x64 is cloned for ARM64, with the linker's
target machine switched over. Everything else in the project is keyed on
$(Configuration) alone and applies unchanged.

The transformation is textual rather than via ElementTree on purpose: msbuild
projects live in a default XML namespace, and a round-trip through ElementTree
rewrites every tag with an ns0: prefix, which makes the result unreadable in a
diff even though msbuild accepts it.

Usage:
    python msdev-add-arm64.py cryptlib.vcxproj [more.vcxproj ...]

Re-running on an already-patched project is a no-op.
"""

import re
import sys

PROJECT_CONFIGURATION = re.compile(
    r'[ \t]*<ProjectConfiguration Include="(?P<config>[^"]+)\|x64">.*?'
    r"</ProjectConfiguration>\r?\n",
    re.DOTALL,
)

ITEM_DEFINITION_GROUP = re.compile(
    r"[ \t]*<ItemDefinitionGroup Condition=\"'\$\(Platform\)'=='x64'\".*?"
    r"</ItemDefinitionGroup>\r?\n",
    re.DOTALL,
)


def to_arm64(block):
    """Rewrite one cloned x64 block so it describes the ARM64 platform."""
    block = block.replace("|x64", "|ARM64")
    block = block.replace("<Platform>x64</Platform>", "<Platform>ARM64</Platform>")
    block = block.replace("'$(Platform)'=='x64'", "'$(Platform)'=='ARM64'")
    block = block.replace(
        "<TargetMachine>MachineX64</TargetMachine>",
        "<TargetMachine>MachineARM64</TargetMachine>",
    )
    block = block.replace('Label="X64 Configuration"', 'Label="ARM64 Configuration"')
    return block


def patch(text):
    """Return (patched text, number of blocks added)."""
    added = 0
    for pattern in (PROJECT_CONFIGURATION, ITEM_DEFINITION_GROUP):
        out = []
        last = 0
        for match in pattern.finditer(text):
            block = match.group(0)
            arm64 = to_arm64(block)
            if arm64 == block or arm64 in text:
                # Either nothing x64-specific to rewrite, or a previous run
                # already added this one.
                continue
            out.append(text[last : match.end()])
            out.append(arm64)
            last = match.end()
            added += 1
        out.append(text[last:])
        text = "".join(out)
    return text, added


def main(argv):
    if not argv:
        print(__doc__, file=sys.stderr)
        return 2
    for path in argv:
        # newline="" keeps the project's CRLF line endings, and the BOM is put
        # back only if it was there: rewriting either would turn a five-block
        # addition into a whole-file diff.
        raw = open(path, "rb").read()
        bom = raw.startswith(b"\xef\xbb\xbf")
        with open(path, "r", encoding="utf-8-sig", newline="") as handle:
            text = handle.read()
        patched, added = patch(text)
        if not added:
            print("%s: already has ARM64 configurations, left alone" % path)
            continue
        with open(
            path, "w", encoding="utf-8-sig" if bom else "utf-8", newline=""
        ) as handle:
            handle.write(patched)
        print("%s: added %d ARM64 block(s)" % (path, added))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
