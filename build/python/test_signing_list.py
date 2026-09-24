#!/usr/bin/env python3
"""Unit tests for signing_list.py. Run with: python build/python/test_signing_list.py"""

import contextlib
import io
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import signing_list  # noqa: E402

REAL_WIX_DIR = os.path.normpath(os.path.join(HERE, '..', '..', 'installers', 'installer-NSCP'))

WXS = """\
<Wix>
  <File Id="A" Name="nscp.exe" Source="$(var.Source)/nscp.exe" />
  <File Id="B"
        Name="CheckDisk.dll"
        Source="$(var.Source)/modules/CheckDisk.dll" />
  <File Id="C" Name="x.pyd" Source="$(var.Source)/$(var.Py).pyd" />
  <File Id="D" Name="crypto" Source="$(var.Source)/$(var.OpenSSLCryptoDll)" />
  <File Id="E" Name="nsclient.ini" Source="$(var.Source)/nsclient.ini" />
  <File Id="F" Name="missing.dll" Source="$(var.Source)/modules/Missing.dll" />
  <!-- <File Id="G" Name="old.dll" Source="$(var.Source)/old.dll" /> -->
  <Binary Id='Helper' SourceFile='$(var.InstallerDllPath)/installer_lib.dll' />
  <Binary Id="Map" SourceFile="$(var.Source)/old-settings.map" />
</Wix>
"""


def touch(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        f.write('x')


class SigningListTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = self.tmp.name
        self.build = os.path.join(self.root, 'build')
        self.wix = os.path.join(self.root, 'wix')
        os.makedirs(self.wix)
        with open(os.path.join(self.wix, 'Product.wxs'), 'w') as f:
            f.write(WXS)
        for name in ('nscp.exe', 'modules/CheckDisk.dll', 'python311.pyd', 'nsclient.ini',
                     'old.dll', 'libcrypto-3-x64.dll',
                     # Built next to nscp.exe, never installed: must not be signed.
                     'nscp_where_filter_test.exe', 'old-settings.map',
                     '../installer_lib/Release/installer_lib.dll'):
            touch(os.path.join(self.build, name))

    def tearDown(self):
        self.tmp.cleanup()

    def listed(self, defines):
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            files = signing_list.signable_files(self.wix, defines)
        rel = [os.path.relpath(f, self.build).replace(os.sep, '/') for f in files]
        return rel, err.getvalue()

    def test_lists_only_installed_binaries(self):
        rel, err = self.listed({'Source': self.build, 'Py': 'python311',
                                'OpenSSLCryptoDll': 'libcrypto-3-x64.dll',
                                'InstallerDllPath': os.path.join(self.root, 'installer_lib', 'Release')})
        self.assertCountEqual(rel, ['../installer_lib/Release/installer_lib.dll', 'libcrypto-3-x64.dll',
                               'modules/CheckDisk.dll', 'nscp.exe', 'python311.pyd'])
        self.assertIn('Missing.dll: not built', err)

    def test_undefined_variable_is_skipped(self):
        rel, err = self.listed({'Source': self.build, 'Py': 'python311'})
        self.assertNotIn('libcrypto-3-x64.dll', rel)
        self.assertIn('$(var.OpenSSLCryptoDll) is not defined', err)

    def test_windows_separators_in_defines(self):
        rel, _ = self.listed({'Source': self.build.replace('/', '\\') if os.sep == '\\' else self.build,
                              'Py': 'python311', 'OpenSSLCryptoDll': 'libcrypto-3-x64.dll'})
        self.assertIn('nscp.exe', rel)

    def test_read_defines(self):
        path = os.path.join(self.root, 'wix-defines.txt')
        with open(path, 'w') as f:
            f.write('Source=C:/b/tmp/nscp\narch=x64\nApp.Title=NSClient++\nWebHelpSource=C:\\b\\docs\\html\n\n')
        self.assertEqual(signing_list.read_defines(path),
                         {'Source': 'C:/b/tmp/nscp', 'arch': 'x64', 'App.Title': 'NSClient++',
                          'WebHelpSource': 'C:\\b\\docs\\html'})

    def test_main_fails_when_nothing_is_found(self):
        path = os.path.join(self.root, 'wix-defines.txt')
        with open(path, 'w') as f:
            f.write('Source=%s\n' % os.path.join(self.root, 'nowhere'))
        with contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(signing_list.main(['--wix-dir', self.wix, '--defines', path]), 1)

    def test_real_installer_sources(self):
        """The shipped .wxs files: the core binaries are found, tests are not."""
        defines = {'Source': self.build, 'BoostPythonVersion': 'python311',
                   'InstallerDllPath': os.path.join(self.root, 'installer_lib', 'Release'),
                   'OpenSSLCryptoDll': 'libcrypto-3-x64.dll', 'OpenSSLSslDll': 'libssl-3-x64.dll'}
        for _, source in signing_list.sources(REAL_WIX_DIR):
            try:
                name = signing_list.resolve(source, defines)
            except signing_list.UndefinedVariable:
                continue  # web, help and config sources use defines not set here
            touch(name)
        # A binary built next to nscp.exe that the installer does not ship.
        touch(os.path.join(self.build, 'nscp_where_filter_test.exe'))
        files = signing_list.signable_files(REAL_WIX_DIR, defines, log=io.StringIO())
        rel = {os.path.relpath(f, self.build).replace(os.sep, '/') for f in files}
        for expected in ('nscp.exe', 'plugin_api.dll', 'modules/CheckSystem.dll',
                         'modules/dotnet/NSCP.Core.dll', 'libcrypto-3-x64.dll', 'python311.dll',
                         '../installer_lib/Release/installer_lib.dll'):
            self.assertIn(expected, rel)
        self.assertNotIn('nscp_where_filter_test.exe', rel)
        self.assertFalse([f for f in rel if not f.endswith(signing_list.SIGNABLE)])


if __name__ == '__main__':
    unittest.main()
