"""Render packaging templates for a published GitHub release.

This helper is shared by the three Windows packaging workflows
(winget / chocolatey / scoop). It:

* downloads the published release assets we are interested in,
* computes their SHA256 checksums,
* and renders every ``*.tmpl`` file under a template directory into an
  output directory, replacing ``{{KEY}}`` placeholders.

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
        --asset MSI_X86=NSCP-0.6.1-Win32.msi
"""

from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import os
import re
import shutil
import sys
import urllib.request
from pathlib import Path
from typing import Dict


GITHUB_RELEASE_DOWNLOAD = (
    "https://github.com/{repo}/releases/download/{tag}/{name}"
)


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


def _render_dir(template_dir: Path, output_dir: Path, subs: Dict[str, str]) -> None:
    if not template_dir.is_dir():
        raise SystemExit(f"Template directory not found: {template_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)

    pattern = re.compile(r"\{\{([A-Z0-9_]+)\}\}")

    def replace(match):
        key = match.group(1)
        if key not in subs:
            raise SystemExit(
                f"Template references undefined placeholder {{{{{key}}}}}"
            )
        return subs[key]

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
            dest.write_text(pattern.sub(replace, text), encoding="utf-8")
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
        default=_dt.date.today().isoformat(),
    )
    parser.add_argument(
        "--extra",
        action="append",
        default=[],
        metavar="KEY=VALUE",
    )
    args = parser.parse_args(argv)

    assets = _parse_kv(args.asset)
    extras = _parse_kv(args.extra)

    subs: Dict[str, str] = {
        "VERSION": args.version,
        "RELEASE_TAG": args.release_tag,
        "RELEASE_DATE": args.release_date,
    }
    subs.update(extras)

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
