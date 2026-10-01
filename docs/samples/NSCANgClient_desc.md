<!-- @formatter:off -->
!!! warning "Not included in the Windows installer: install it by hand"
    The Windows installer (MSI) does not install this module. A few antivirus
    engines (Avast and AVG, Avira and WithSecure, Sophos, Trellix) report
    `NSCANgClient.dll` as malware, with generic machine-learning verdicts such
    as `Win64:Evo-gen [Trj]`, `TR/W64.Evo`, `Mal/Generic-S` and `Artemis!`.
    Generic verdicts like these are typical of false positives, but they made
    the whole installer look infected, so the module was taken out of it until
    the vendors have analysed it. The DLL is built by the release workflow from
    this repository, Authenticode-signed like every other binary, and covered
    by the release's
    [build provenance attestation](../../setup/installing.md#verifying-the-download).
    The [FAQ](../../faq.md#112-my-antivirus-reports-nsclient-as-a-virus)
    explains how to check the file, what those checks cannot tell you, and
    where to report a detection.

    The module still ships in the Windows zip, and the Linux and macOS packages
    are unaffected. To add it to an agent installed from the MSI:

    1. Download `NSCP-<version>-<platform>.zip` from the
       [releases page](https://github.com/mickem/nscp/releases) for **the same
       version and platform** as the installed agent. A module built for
       another version is not expected to work with it.
    2. Check the DLL's signature before you copy it:
       `Get-AuthenticodeSignature .\modules\NSCANgClient.dll` must report
       `Valid`. That shows the file is the one the release workflow built, not
       that its contents are safe.
    3. Copy `modules\NSCANgClient.dll` from the zip into the `modules` folder
       of the installation, by default `C:\Program Files\NSClient++\modules\`.
       If your antivirus quarantines it, send the file to your vendor for
       analysis. Whether to exclude it before they answer is your call; if you
       do, exclude that one file, not the folder.
    4. Enable the module as shown below and restart the service.

    The installer does not own a file copied this way, so an upgrade neither
    replaces nor removes it. **Copy the matching DLL again after every
    upgrade.** Upgrading from a release whose installer still shipped the
    module removes the installed copy, so copy it in after that upgrade too.
<!-- @formatter:on -->
