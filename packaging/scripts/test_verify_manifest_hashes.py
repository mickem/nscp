"""Unit tests for verify_manifest_hashes.py.

Tests only the manifest-parsing layer. The HTTP fetch is stubbed via
``urllib.request.urlopen`` so no network is touched.
"""

from __future__ import annotations

import contextlib
import hashlib
import io
import shutil
import tempfile
import unittest
import urllib.request
from pathlib import Path

import verify_manifest_hashes as vmh


@contextlib.contextmanager
def _tmpdir():
    d = Path(tempfile.mkdtemp(prefix="vmh-test-"))
    try:
        yield d
    finally:
        shutil.rmtree(d, ignore_errors=True)


@contextlib.contextmanager
def _stub_urlopen(payloads: dict[str, bytes]):
    """Stub urlopen so it returns the bytes registered for each URL."""
    real = urllib.request.urlopen

    def fake(req, *a, **kw):
        url = req.get_full_url() if hasattr(req, "get_full_url") else req
        if url not in payloads:
            raise SystemExit(f"unexpected URL fetched in test: {url}")

        class _Resp(io.BytesIO):
            def __enter__(self):
                return self

            def __exit__(self, *exc):
                self.close()
                return False

        return _Resp(payloads[url])

    urllib.request.urlopen = fake
    try:
        yield
    finally:
        urllib.request.urlopen = real


def _sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest().upper()


class WingetExtractorTests(unittest.TestCase):
    def test_pairs_extracted_in_file_order(self):
        with _tmpdir() as d:
            (d / "Pkg.installer.yaml").write_text(
                "Installers:\n"
                "  - Architecture: x64\n"
                "    InstallerUrl: https://example.com/a.msi\n"
                "    InstallerSha256: " + "A" * 64 + "\n"
                "  - Architecture: x86\n"
                "    InstallerUrl: https://example.com/b.msi\n"
                "    InstallerSha256: " + "B" * 64 + "\n"
            )
            pairs = vmh._pairs_winget(d)
            self.assertEqual(
                pairs,
                [
                    ("https://example.com/a.msi", "A" * 64),
                    ("https://example.com/b.msi", "B" * 64),
                ],
            )

    def test_missing_installer_yaml_exits(self):
        with _tmpdir() as d, self.assertRaises(SystemExit):
            vmh._pairs_winget(d)

    def test_url_hash_count_mismatch_exits(self):
        with _tmpdir() as d:
            (d / "x.installer.yaml").write_text(
                "InstallerUrl: https://example.com/a.msi\n"
                "InstallerSha256: " + "A" * 64 + "\n"
                "InstallerUrl: https://example.com/b.msi\n"
            )
            with self.assertRaises(SystemExit):
                vmh._pairs_winget(d)


class ChocolateyExtractorTests(unittest.TestCase):
    def test_both_arches(self):
        with _tmpdir() as d:
            (d / "tools").mkdir()
            (d / "tools" / "chocolateyInstall.ps1").write_text(
                "$packageArgs = @{\n"
                "  url            = 'https://example.com/x86.msi'\n"
                "  url64bit       = 'https://example.com/x64.msi'\n"
                "  checksum       = '" + "C" * 64 + "'\n"
                "  checksum64     = '" + "D" * 64 + "'\n"
                "}\n"
            )
            pairs = vmh._pairs_chocolatey(d)
            self.assertEqual(
                set(pairs),
                {
                    ("https://example.com/x86.msi", "C" * 64),
                    ("https://example.com/x64.msi", "D" * 64),
                },
            )

    def test_missing_install_ps1_exits(self):
        with _tmpdir() as d, self.assertRaises(SystemExit):
            vmh._pairs_chocolatey(d)


class ScoopExtractorTests(unittest.TestCase):
    def test_both_arches(self):
        with _tmpdir() as d:
            (d / "nscp.json").write_text(
                '{"architecture": {'
                '  "64bit": {"url": "https://example.com/x64.zip", "hash": "' + "E" * 64 + '"},'
                '  "32bit": {"url": "https://example.com/x86.zip", "hash": "' + "F" * 64 + '"}'
                '}}'
            )
            pairs = vmh._pairs_scoop(d)
            self.assertIn(("https://example.com/x64.zip", "E" * 64), pairs)
            self.assertIn(("https://example.com/x86.zip", "F" * 64), pairs)

    def test_missing_hash_exits(self):
        with _tmpdir() as d:
            (d / "nscp.json").write_text(
                '{"architecture": {"64bit": {"url": "https://example.com/x.zip"}}}'
            )
            with self.assertRaises(SystemExit):
                vmh._pairs_scoop(d)


class MainTests(unittest.TestCase):
    def test_passes_when_hash_matches(self):
        with _tmpdir() as d:
            payload = b"hello-world"
            digest = _sha(payload)
            (d / "p.installer.yaml").write_text(
                f"InstallerUrl: https://example.com/p.msi\n"
                f"InstallerSha256: {digest}\n"
            )
            with _stub_urlopen({"https://example.com/p.msi": payload}):
                rc = vmh.main(["winget", str(d)])
            self.assertEqual(rc, 0)

    def test_fails_when_hash_mismatches(self):
        with _tmpdir() as d:
            (d / "p.installer.yaml").write_text(
                "InstallerUrl: https://example.com/p.msi\n"
                "InstallerSha256: " + "0" * 64 + "\n"
            )
            with _stub_urlopen({"https://example.com/p.msi": b"different"}):
                rc = vmh.main(["winget", str(d)])
            self.assertEqual(rc, 1)

    def test_unknown_kind_returns_usage_exit(self):
        rc = vmh.main(["bogus", "."])
        self.assertEqual(rc, 2)


if __name__ == "__main__":
    unittest.main()
