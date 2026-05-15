"""Round-trip verifier for rendered package manifests.

For each (URL, expected SHA256) pair embedded in a rendered manifest, fetch
the URL and confirm the bytes hash to the expected value. Catches:

* URL template drift (e.g. asset name change in release.yml not mirrored in
  the template).
* Missing release assets (404 from github.com).
* A renderer bug that writes a stale or mismatched hash.

Stdlib only — runs on the bare ``windows-latest`` / ``ubuntu-latest``
runners with no ``pip install`` step.

Usage::

    python verify_manifest_hashes.py winget     dist/winget
    python verify_manifest_hashes.py chocolatey dist/chocolatey
    python verify_manifest_hashes.py scoop      dist/scoop
"""

from __future__ import annotations

import hashlib
import json
import re
import sys
import urllib.request
from pathlib import Path


CHUNK = 1 << 20


def _sha256_url(url: str) -> str:
    req = urllib.request.Request(url, headers={"User-Agent": "nscp-packaging-verify"})
    h = hashlib.sha256()
    with urllib.request.urlopen(req) as resp:
        for chunk in iter(lambda: resp.read(CHUNK), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def _pairs_winget(manifest_dir: Path) -> list[tuple[str, str]]:
    # The installer manifest is the only file that carries URLs + hashes.
    # We pair them positionally — every URL is immediately followed by its
    # SHA256 in the rendered file.
    pairs: list[tuple[str, str]] = []
    installer_yaml = next(manifest_dir.glob("*installer.yaml"), None)
    if installer_yaml is None:
        raise SystemExit(f"No *installer.yaml under {manifest_dir}")
    text = installer_yaml.read_text(encoding="utf-8")
    url_re = re.compile(r"^\s*InstallerUrl:\s*(\S+)\s*$", re.MULTILINE)
    sha_re = re.compile(r"^\s*InstallerSha256:\s*([0-9A-Fa-f]{64})\s*$", re.MULTILINE)
    urls = url_re.findall(text)
    shas = sha_re.findall(text)
    if len(urls) != len(shas):
        raise SystemExit(
            f"winget manifest has {len(urls)} URLs but {len(shas)} hashes"
        )
    if not urls:
        raise SystemExit("winget manifest has no Installers entries")
    pairs.extend(zip(urls, shas))
    return pairs


def _pairs_chocolatey(manifest_dir: Path) -> list[tuple[str, str]]:
    ps1 = manifest_dir / "tools" / "chocolateyInstall.ps1"
    if not ps1.exists():
        raise SystemExit(f"Missing {ps1}")
    text = ps1.read_text(encoding="utf-8")
    # Extract single-quoted values for the four known keys.
    def grab(key: str) -> str | None:
        m = re.search(rf"{key}\s*=\s*'([^']+)'", text)
        return m.group(1) if m else None
    url, url64 = grab("url"), grab("url64bit")
    sha, sha64 = grab("checksum"), grab("checksum64")
    pairs: list[tuple[str, str]] = []
    if url and sha:
        pairs.append((url, sha))
    if url64 and sha64:
        pairs.append((url64, sha64))
    if not pairs:
        raise SystemExit("chocolateyInstall.ps1 has no url/checksum pairs")
    return pairs


def _pairs_scoop(manifest_dir: Path) -> list[tuple[str, str]]:
    js = manifest_dir / "nscp.json"
    if not js.exists():
        raise SystemExit(f"Missing {js}")
    data = json.loads(js.read_text(encoding="utf-8"))
    pairs: list[tuple[str, str]] = []
    arch = data.get("architecture", {})
    for variant, entry in arch.items():
        url = entry.get("url")
        h = entry.get("hash")
        if not url or not h:
            raise SystemExit(f"scoop {variant} missing url or hash")
        pairs.append((url, h))
    if not pairs:
        raise SystemExit("scoop manifest has no architecture entries")
    return pairs


EXTRACTORS = {
    "winget": _pairs_winget,
    "chocolatey": _pairs_chocolatey,
    "scoop": _pairs_scoop,
}


def main(argv: list[str] | None = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if len(argv) != 2 or argv[0] not in EXTRACTORS:
        print(__doc__, file=sys.stderr)
        return 2
    kind, manifest_dir = argv[0], Path(argv[1])

    pairs = EXTRACTORS[kind](manifest_dir)
    print(f"Verifying {len(pairs)} URL/hash pair(s) for {kind}:")
    failures: list[str] = []
    for url, expected in pairs:
        expected_up = expected.upper()
        print(f"  -> {url}")
        try:
            actual = _sha256_url(url)
        except Exception as exc:
            failures.append(f"{url}: download failed ({exc})")
            continue
        if actual != expected_up:
            failures.append(
                f"{url}: hash mismatch\n      expected {expected_up}\n      actual   {actual}"
            )
        else:
            print(f"     ok ({actual})")

    if failures:
        for f in failures:
            print(f"::error::{f}")
        return 1
    print("All hashes verified.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
