"""Unit tests for render_templates.py.

Stdlib-only (matches the renderer's own no-third-party-deps constraint).
Run with: ``python -m unittest packaging/scripts/test_render_templates.py``.
"""

from __future__ import annotations

import contextlib
import hashlib
import shutil
import tempfile
import unittest
from pathlib import Path

# Importable from the same directory.
import render_templates as rt


def _seed_asset(path: Path, payload: bytes) -> str:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)
    return hashlib.sha256(payload).hexdigest().upper()


@contextlib.contextmanager
def _tmpdir():
    d = Path(tempfile.mkdtemp(prefix="rt-test-"))
    try:
        yield d
    finally:
        shutil.rmtree(d, ignore_errors=True)


class RenderTemplatesTests(unittest.TestCase):
    def _run(self, templates: Path, output: Path, *extra: str, download_dir: Path | None = None):
        argv = [
            "--version", "1.2.3",
            "--release-tag", "1.2.3",
            "--repo", "example/repo",
            "--templates", str(templates),
            "--output", str(output),
            "--release-date", "2026-01-02",
        ]
        if download_dir is not None:
            argv += ["--download-dir", str(download_dir)]
        argv += list(extra)
        return rt.main(argv)

    def test_happy_path_renders_tmpl_and_copies_non_tmpl(self):
        with _tmpdir() as work:
            tpl = work / "tpl"
            tpl.mkdir()
            (tpl / "manifest.yaml.tmpl").write_text(
                "version: {{VERSION}}\ntag: {{RELEASE_TAG}}\ndate: {{RELEASE_DATE}}\n"
            )
            (tpl / "static.txt").write_bytes(b"copy-me-verbatim")

            out = work / "out"
            dl = work / "_assets"

            rc = self._run(tpl, out, download_dir=dl)
            self.assertEqual(rc, 0)

            rendered = (out / "manifest.yaml").read_text()
            self.assertEqual(
                rendered, "version: 1.2.3\ntag: 1.2.3\ndate: 2026-01-02\n"
            )
            # .tmpl suffix is stripped; non-.tmpl files are copied verbatim.
            self.assertEqual((out / "static.txt").read_bytes(), b"copy-me-verbatim")

    def test_asset_url_and_hash_substitution(self):
        # Seed an asset locally so the renderer's "already downloaded" branch
        # is hit and no HTTP request is made.
        with _tmpdir() as work:
            tpl = work / "tpl"
            tpl.mkdir()
            (tpl / "out.txt.tmpl").write_text(
                "u={{URL_MSI_X64}}\nh={{SHA256_MSI_X64}}\n"
            )

            out = work / "out"
            dl = work / "_assets"
            sha = _seed_asset(dl / "foo.msi", b"hello-world")

            rc = self._run(
                tpl, out,
                "--asset", "MSI_X64=foo.msi",
                download_dir=dl,
            )
            self.assertEqual(rc, 0)

            text = (out / "out.txt").read_text()
            self.assertIn(
                "u=https://github.com/example/repo/releases/download/1.2.3/foo.msi",
                text,
            )
            self.assertIn(f"h={sha}", text)
            # Hash must be uppercase — Chocolatey/winget expect it.
            self.assertTrue(sha.isupper())
            self.assertEqual(len(sha), 64)

    def test_undefined_placeholder_exits_nonzero(self):
        with _tmpdir() as work:
            tpl = work / "tpl"
            tpl.mkdir()
            (tpl / "bad.tmpl").write_text("hello {{NOPE}}")
            out = work / "out"
            dl = work / "_assets"

            with self.assertRaises(SystemExit) as cm:
                self._run(tpl, out, download_dir=dl)
            # SystemExit carries the message; assert it names the bad key.
            self.assertIn("NOPE", str(cm.exception))

    def test_malformed_asset_argument_exits_nonzero(self):
        with _tmpdir() as work:
            tpl = work / "tpl"
            tpl.mkdir()
            (tpl / "x.tmpl").write_text("v={{VERSION}}")
            with self.assertRaises(SystemExit):
                self._run(
                    tpl, work / "out",
                    "--asset", "NO_EQUALS_SIGN",
                    download_dir=work / "_assets",
                )

    def test_lowercase_placeholder_is_left_literal(self):
        # The renderer's regex matches only [A-Z0-9_]+. A lowercase token must
        # pass through untouched — this is a known foot-gun documented in
        # docs/design/windows-packaging-fixes.md.
        with _tmpdir() as work:
            tpl = work / "tpl"
            tpl.mkdir()
            (tpl / "x.tmpl").write_text("v={{Version}}")
            out = work / "out"
            rc = self._run(tpl, out, download_dir=work / "_assets")
            self.assertEqual(rc, 0)
            self.assertEqual((out / "x").read_text(), "v={{Version}}")

    def test_nested_directory_structure_is_preserved(self):
        # Mirrors the chocolatey layout: tools/ subdir with a mix of .tmpl
        # and non-.tmpl files.
        with _tmpdir() as work:
            tpl = work / "tpl"
            (tpl / "tools").mkdir(parents=True)
            (tpl / "nuspec.tmpl").write_text("<v>{{VERSION}}</v>")
            (tpl / "tools" / "install.ps1.tmpl").write_text("# v {{VERSION}}")
            (tpl / "tools" / "uninstall.ps1").write_bytes(b"# verbatim")

            out = work / "out"
            rc = self._run(tpl, out, download_dir=work / "_assets")
            self.assertEqual(rc, 0)

            self.assertEqual((out / "nuspec").read_text(), "<v>1.2.3</v>")
            self.assertEqual(
                (out / "tools" / "install.ps1").read_text(), "# v 1.2.3"
            )
            self.assertEqual(
                (out / "tools" / "uninstall.ps1").read_bytes(), b"# verbatim"
            )

    def test_sha256_helper_streams_large_files(self):
        # Sanity-check the chunked reader against hashlib's one-shot result
        # for a file larger than the 1 MiB read chunk.
        with _tmpdir() as work:
            payload = b"A" * (1024 * 1024 + 17)
            f = work / "big.bin"
            f.write_bytes(payload)
            expected = hashlib.sha256(payload).hexdigest().upper()
            self.assertEqual(rt._sha256(f), expected)

    def test_extra_kv_substitutions(self):
        with _tmpdir() as work:
            tpl = work / "tpl"
            tpl.mkdir()
            (tpl / "x.tmpl").write_text("hi {{CUSTOM_FIELD}}")
            out = work / "out"
            rc = self._run(
                tpl, out,
                "--extra", "CUSTOM_FIELD=hello",
                download_dir=work / "_assets",
            )
            self.assertEqual(rc, 0)
            self.assertEqual((out / "x").read_text(), "hi hello")

    def test_missing_template_dir_exits_nonzero(self):
        with _tmpdir() as work:
            with self.assertRaises(SystemExit):
                self._run(
                    work / "nope", work / "out",
                    download_dir=work / "_assets",
                )


if __name__ == "__main__":
    unittest.main()
