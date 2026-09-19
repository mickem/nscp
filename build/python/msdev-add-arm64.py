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

One source file has to be added as well. neon_simd.cpp is absent from
cryptlib.vcxproj -- it is the only non-PowerPC, non-test source the project
leaves out -- because nothing on x86 or x64 needs it. On ARM64 it does:
cpu.cpp calls CPU_ProbeNEON(), which neon_simd.cpp defines (as a plain
`return true` under _M_ARM64), so without it every consumer of cryptlib fails
with "LNK2019: unresolved external symbol CryptoPP::CPU_ProbeNEON". The entry
is conditioned on the ARM64 platform, so an x86 or x64 build of a patched tree
compiles exactly the same set of files it does today. The file defines only
CPU_ProbeNEON and CPU_ProbeARMv7, neither of which exists elsewhere in the
project, so it adds no duplicate symbols.

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

# Any self-closing ClCompile entry, used to find where the source list ends.
CLCOMPILE = re.compile(r"([ \t]*)<ClCompile Include=\"[^\"]+\"\s*/>(\r?\n)")

NEON_SOURCE = "neon_simd.cpp"

# What counts as "already added": a ClCompile item for the file, not the mere
# appearance of the name. Upstream could list it as a <None> item, name it in
# a comment or ship it under a <ClInclude>, and a bare substring test would
# then skip the insertion and leave CPU_ProbeNEON unresolved at link time -
# silently, and only on ARM64.
NEON_CLCOMPILE = re.compile(
    r"<ClCompile\s+Include=\"(?:[^\"]*[\\/])?%s\"" % re.escape(NEON_SOURCE),
    re.I,
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


def add_neon_source(text):
    """Add neon_simd.cpp to the source list, conditioned on ARM64."""
    if NEON_CLCOMPILE.search(text):
        return text, 0
    entries = list(CLCOMPILE.finditer(text))
    if not entries:
        return text, 0
    # Appended after the last entry rather than inserted near the front: the
    # project notes that the order of the first three sources matters.
    last = entries[-1]
    indent, newline = last.group(1), last.group(2)
    entry = '%s<ClCompile Include="%s" Condition="\'$(Platform)\'==\'ARM64\'" />%s' % (
        indent,
        NEON_SOURCE,
        newline,
    )
    return text[: last.end()] + entry + text[last.end() :], 1


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
    text, neon = add_neon_source(text)
    added += neon
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
