"""Render packaging templates for a published GitHub release.

This helper is shared by the three Windows packaging workflows
(winget / chocolatey / scoop). It:

* downloads the published release assets we are interested in,
* computes their SHA256 checksums,
* and renders every ``*.tmpl`` file under a template directory into an
  output directory, replacing ``{{KEY}}`` placeholders.

A placeholder that sits alone on its line may expand to a multi-line value:
every line after the first is indented to the placeholder's own column, which
is exactly what a YAML block scalar needs::

    ReleaseNotes: |-
      {{RELEASE_NOTES}}

The script intentionally has no third-party dependencies so it can run on
``windows-latest`` and ``ubuntu-latest`` GitHub runners without any
``pip install`` step.

Example::

    python packaging/scripts/render_templates.py \\
        --version 0.6.1 \\
        --release-tag 0.6.1 \\
        --templates packaging/winget \\
        --output dist/winget \\
        --asset MSI_X64=NSCP-0.6.1-x64.msi \\
        --asset MSI_X86=NSCP-0.6.1-Win32.msi \\
        --extra-file RELEASE_NOTES=dist/release-notes.md
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import os
import re
import shutil
import sys
import urllib.request
from pathlib import Path


GITHUB_RELEASE_DOWNLOAD = (
    "https://github.com/{repo}/releases/download/{tag}/{name}"
)

# WinGet caps ReleaseNotes at 10000 characters, and a manifest that busts the
# cap is rejected by `winget validate` rather than trimmed, so --extra-file
# values are truncated to fit by default.
DEFAULT_EXTRA_FILE_MAX_CHARS = 10000

TRUNCATION_MARKER = "\n\n[...] (truncated)"

# Cutting at a line boundary can leave a heading with nothing under it, which
# reads as a section that lost its body rather than as a document that stops.
_TRAILING_HEADING = re.compile(r"\n#{1,6} [^\n]*$")

_PLACEHOLDER = re.compile(r"\{\{([A-Z0-9_]+)\}\}")

# A placeholder that owns its whole line; only these may expand to a
# multi-line value, because only there do we know the indentation to reuse.
_SOLE_PLACEHOLDER = re.compile(r"^([ \t]*)\{\{([A-Z0-9_]+)\}\}[ \t\r]*$")


def _download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    print(f"  -> downloading {url}")
    req = urllib.request.Request(url, headers={"User-Agent": "nscp-packaging"})
    with urllib.request.urlopen(req) as resp, dest.open("wb") as out:
        shutil.copyfileobj(resp, out)


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fp:
        for chunk in iter(lambda: fp.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def _parse_kv(items):
    parsed = {}
    for item in items or []:
        if "=" not in item:
            raise SystemExit(f"expected KEY=VALUE, got: {item!r}")
        key, value = item.split("=", 1)
        parsed[key.strip()] = value.strip()
    return parsed


def _normalize_newlines(text: str) -> str:
    return text.replace("\r\n", "\n").replace("\r", "\n")


def _truncate(text: str, limit: int) -> str:
    """Trim *text* to at most *limit* characters, preferring a line boundary."""
    if limit <= 0 or len(text) <= limit:
        return text
    budget = limit - len(TRUNCATION_MARKER)
    if budget <= 0:
        return text[:limit]
    head = text[:budget]
    cut = head.rfind("\n")
    if cut > budget // 2:
        head = head[:cut]
    head = head.rstrip()
    while True:
        trimmed = _TRAILING_HEADING.sub("", head).rstrip()
        if trimmed == head:
            return head + TRUNCATION_MARKER
        head = trimmed


def _substitute(text: str, subs: dict[str, str]) -> str:
    """Replace every ``{{KEY}}`` placeholder in *text*.

    A placeholder alone on its line expands with its own indentation applied
    to every continuation line, so a multi-line value drops straight into a
    YAML block scalar. Elsewhere a multi-line value would silently produce
    broken YAML, so it is rejected instead.
    """

    def value_of(key: str) -> str:
        if key not in subs:
            raise SystemExit(
                f"Template references undefined placeholder {{{{{key}}}}}"
            )
        return subs[key]

    def replace_inline(match):
        value = value_of(match.group(1))
        if "\n" in value or "\r" in value:
            raise SystemExit(
                f"Placeholder {{{{{match.group(1)}}}}} holds a multi-line "
                "value but shares its line with other text; put it on a line "
                "of its own so its indentation can be reused."
            )
        return value

    rendered = []
    for line in text.split("\n"):
        sole = _SOLE_PLACEHOLDER.match(line)
        if not sole:
            rendered.append(_PLACEHOLDER.sub(replace_inline, line))
            continue
        indent, key = sole.group(1), sole.group(2)
        value = _normalize_newlines(value_of(key)).split("\n")
        rendered.append(indent + value[0])
        # Blank lines stay blank: trailing whitespace in a block scalar is
        # noise, and YAML does not need it to keep the paragraph together.
        rendered.extend(indent + ln if ln.strip() else "" for ln in value[1:])
    return "\n".join(rendered)


def _render_dir(template_dir: Path, output_dir: Path, subs: dict[str, str]) -> None:
    if not template_dir.is_dir():
        raise SystemExit(f"Template directory not found: {template_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)

    for src in template_dir.rglob("*"):
        if src.is_dir():
            continue
        rel = src.relative_to(template_dir)
        if rel.suffix == ".tmpl":
            rel = rel.with_suffix("")
        dest = output_dir / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        if src.suffix == ".tmpl":
            text = src.read_text(encoding="utf-8")
            dest.write_text(_substitute(text, subs), encoding="utf-8")
        else:
            shutil.copyfile(src, dest)
        print(f"  -> rendered {rel}")


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--release-tag", required=True)
    parser.add_argument(
        "--repo",
        default=os.environ.get("GITHUB_REPOSITORY", "mickem/nscp"),
        help="owner/name of the GitHub repository (defaults to $GITHUB_REPOSITORY).",
    )
    parser.add_argument("--templates", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--download-dir",
        type=Path,
        default=Path("dist/_assets"),
    )
    parser.add_argument(
        "--asset",
        action="append",
        default=[],
        metavar="KEY=FILENAME",
        help=(
            "Declare a release asset to download. KEY becomes the suffix used "
            "in the URL_<KEY> and SHA256_<KEY> placeholders."
        ),
    )
    parser.add_argument(
        "--release-date",
        default=dt.date.today().isoformat(),
    )
    parser.add_argument(
        "--extra",
        action="append",
        default=[],
        metavar="KEY=VALUE",
    )
    parser.add_argument(
        "--extra-file",
        action="append",
        default=[],
        metavar="KEY=PATH",
        help=(
            "Read the value for the {{KEY}} placeholder from a file. Use this "
            "for multi-line values such as release notes."
        ),
    )
    parser.add_argument(
        "--extra-file-max-chars",
        type=int,
        default=DEFAULT_EXTRA_FILE_MAX_CHARS,
        help=(
            "Truncate --extra-file values longer than this (0 disables). "
            "Defaults to WinGet's 10000-character ReleaseNotes limit."
        ),
    )
    args = parser.parse_args(argv)

    assets = _parse_kv(args.asset)
    extras = _parse_kv(args.extra)
    extra_files = _parse_kv(args.extra_file)

    subs: dict[str, str] = {
        "VERSION": args.version,
        "RELEASE_TAG": args.release_tag,
        "RELEASE_DATE": args.release_date,
    }
    subs.update(extras)

    for key, filename in extra_files.items():
        path = Path(filename)
        if not path.is_file():
            raise SystemExit(f"--extra-file {key}: no such file: {path}")
        value = _normalize_newlines(path.read_text(encoding="utf-8")).strip()
        value = _truncate(value, args.extra_file_max_chars)
        subs[key] = value
        print(f"  * {key}: {len(value)} characters from {path}")

    args.download_dir.mkdir(parents=True, exist_ok=True)

    for key, filename in assets.items():
        url = GITHUB_RELEASE_DOWNLOAD.format(
            repo=args.repo, tag=args.release_tag, name=filename
        )
        local = args.download_dir / filename
        if not local.exists():
            _download(url, local)
        digest = _sha256(local)
        subs[f"URL_{key}"] = url
        subs[f"SHA256_{key}"] = digest
        print(f"  * {filename}: {digest}")

    _render_dir(args.templates, args.output, subs)
    return 0


if __name__ == "__main__":
    sys.exit(main())
