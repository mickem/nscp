---
icon: "🔒"
modules: [NSCAServer, NSCAClient]
---
**NSCA: an unrecognized `encryption` value is now a hard error instead of
silently running without encryption.** A typo'd algorithm name (`aes-256`),
or one not compiled into the build, used to fall back to *no encryption* on
the end carrying it. Since the ciphers must match, a one-sided typo showed
up as the peer rejecting every submission with a CRC error rather than as
accepted plaintext — but that failure gave no hint of its cause, and a
value broken the same way on both ends did run plaintext while looking
encrypted. Now the `NSCAServer` module refuses to load and an `NSCAClient`
submission fails, each naming the problem and listing the available
algorithms. Default installs (`aes256`) are unaffected. **Breaking** only
for setups relying on the fallback: fix the algorithm name, or set
`encryption = none` explicitly if plaintext was intended — this includes
builds compiled without crypto++, where any cipher name previously
degraded to plaintext and the server now refuses to start.
See the [security notice](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
