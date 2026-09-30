---
title: "NSCANgClient: antivirus false positives, and the module leaves the Windows installer"
fixed_in: next
severity: "None: a false positive, no vulnerability"
modules: [NSCANgClient]
action: conditional
---
Four antivirus engines on VirusTotal report `NSCANgClient.dll` from the 0.24.0
Windows build as malware: `Win64:Evo-gen [Trj]` (Avast and AVG, which share an
engine), `TR/W64.Evo` (Avira, and WithSecure, which uses Avira's engine),
`Mal/Generic-S` (Sophos) and `Artemis!<hash>` (Trellix). The other engines,
Microsoft Defender among them, report it as clean. All four names are generic
machine-learning verdicts rather than signatures for known malware: they score
what a file looks like, and a small, new, rarely seen DLL whose code is mostly
a TLS handshake over raw sockets is the shape they over-fire on. Nothing in the
module was changed by anyone but this project, and it does nothing beyond what
[its reference page](../reference/client/NSCANgClient.md) documents: submit
passive results to the NSCA-NG server it is configured for.

The file in the release is the one the release workflow built:

* it is Authenticode-signed with the project's certificate, like every
  executable and DLL of the release;
* the MSI and the zip carry a Sigstore-signed build provenance attestation
  naming the commit they were built from (`gh attestation verify <file> --repo
  mickem/nscp`);
* the zip's `SHA256SUMS` covers the DLL.

Because the reports made the whole installer look infected, which is enough
for a download to be blocked, the Windows installer no longer ships the module.
It is still in the Windows zip, still signed, and is copied into the `modules`
folder by hand; the Linux and macOS packages are unchanged. The module goes
back into the installer once the vendors have cleared it.

**What to do:** if you use `NSCANgClient` on Windows, copy
`modules\NSCANgClient.dll` from the zip of the same version and platform into
the installation's `modules` folder after upgrading, and again after every
later upgrade, as the
[NSCANgClient reference](../reference/client/NSCANgClient.md) describes.
Verify the file first, as the
[FAQ](../faq.md#112-my-antivirus-reports-nsclient-as-a-virus) shows. If your
antivirus quarantines it, exclude that one file rather than the NSClient++
folder, and report it to your vendor as a false positive. Nothing to do
otherwise.
