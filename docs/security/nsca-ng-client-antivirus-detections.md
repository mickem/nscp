---
title: "NSCANgClient: antivirus detections, and the module leaves the Windows installer"
fixed_in: next
severity: "Unconfirmed: generic antivirus detections, no known vulnerability"
modules: [NSCANgClient]
action: conditional
---
Four antivirus engines on VirusTotal report `NSCANgClient.dll` from the 0.24.0
Windows build as malware: `Win64:Evo-gen [Trj]` (Avast and AVG, which share an
engine), `TR/W64.Evo` (Avira, and WithSecure, which uses Avira's engine),
`Mal/Generic-S` (Sophos) and `Artemis!<hash>` (Trellix). The other engines,
Microsoft Defender among them, reported it as clean.

All four are generic machine-learning verdicts rather than signatures for a
known malware family: they score what a file looks like, and a small, rarely
seen DLL whose code is mostly a TLS handshake over raw sockets is a shape they
are known to over-fire on. That makes a false positive the likely explanation,
but it is not proof of one, and neither is the file's signature. Until the
vendors have analysed the file, the detections are unconfirmed rather than
disproven.

#### What the release evidence shows

The file in the release is the one this project's release workflow built:

* it is Authenticode-signed with the project's certificate, like every
  executable and DLL of the release;
* the MSI and the zip carry a Sigstore-signed build provenance attestation
  naming the commit they were built from (`gh attestation verify <file> --repo
  mickem/nscp`);
* the zip's `SHA256SUMS` covers the DLL.

That rules out a file altered after the build: a tampered download, mirror or
installed copy. It cannot rule out a compromise before the signature, such as
malicious code in the repository, a compromised dependency or a compromised
build runner, because the result would be signed and attested all the same.
The attestation names the commit, so the source the DLL was built from can be
read, and the SBOM names every third-party component and the upstream file it
was verified against; see
[Verifying the download](../setup/installing.md#verifying-the-download).

#### What changes

Because the reports made the whole installer look infected, which is enough
for a download to be blocked, the Windows installer no longer ships the module.
It is still in the Windows zip, still signed, and is copied into the `modules`
folder by hand; the Linux and macOS packages are unchanged. The module goes
back into the installer once the vendors have cleared it.

**What to do:** if you use `NSCANgClient` on Windows, decide whether to keep
using it while the detections are unconfirmed. If you do, copy
`modules\NSCANgClient.dll` from the zip of the same version and platform into
the installation's `modules` folder after upgrading, and again after every
later upgrade, as the
[NSCANgClient reference](../reference/client/NSCANgClient.md) describes. Check
the file first as the
[FAQ](../faq.md#112-my-antivirus-reports-nsclient-as-a-virus) shows, and read
there what those checks cannot tell you. If your antivirus quarantines it,
send it to your vendor for analysis, and exclude only that one file if you
choose to run it. Report anything that suggests a real compromise privately
through a
[GitHub security advisory](https://github.com/mickem/nscp/security/advisories/new).
Nothing to do otherwise.
