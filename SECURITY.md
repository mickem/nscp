# Security Policy

| Version | Supported | Notes
| --- | --- | --- |
| Latest Release | ✅ | Yes,Please upgrade to the latest stable version. |
| Pre-releases | ⚠️ | Limited,"Safe for use, but intended for testing/feedback." |
| Releases in the last 12 months | ⚠️ | Will get fixes for severe security issues (if there is a breaking change, preventing upgrading) |
| Older Versions | ❌ | No, We do not backport security fixes to older versions unless requested. |

## Reporting a Vulnerability

We prioritize security reports and appreciate the community's help in keeping NSClient++ safe.

1. **GitHub (Preferred)**: Please use the [GitHub Security Advisory tool](https://github.com/mickem/nscp/security/advisories/new). This allows for private collaboration and disclosure.
2. **Email**: You may also email the author directly at `michael at medin dot name`.

> **Note:** Due to aggressive spam filtering, emails occasionally get lost. If you do not receive a reply within **48 hours**, please assume the email was filtered and kindly re-send it or, preferably, open a ticket via the GitHub link above.

## Patching & Release Cadence

### Release Schedule
There is no fixed cadence for updates, but we generally target **one release per month**.
* **Vulnerabilities:** Addressed immediately in a hotfix release.
* **Other security issues:** Addressed in the next scheduled maintenance release.

### Pre-releases
We frequently publish pre-releases. While these contain new code, they are generally safe to use and are not considered "experimental" in terms of stability. However, due to the complexity of the environments NSClient++ operates in, we rely on these releases to validate new features across different system configurations.

## Supply Chain & Dependencies

We strive to review and update dependencies at least every **4 months**, and we monitor for security issues on a daily basis. You can review the exact versions used in our build pipeline configuration [here](https://github.com/mickem/nscp/blob/main/.github/workflows/build-windows.yml#L30-L39).

If you identify a vulnerability in one of our dependencies that impacts NSClient++, please report it to us.

**Dependency Security References:**
* [OpenSSL](https://openssl-library.org/news/vulnerabilities/index.html)
* [Mongoose Web Server](https://github.com/cesanta/mongoose/releases)
* [Boost C++ Libraries](https://www.boost.org/releases/latest/)
* [Python](https://mail.python.org/archives/list/security-announce@python.org/latest)
* [Google Protobuf](https://github.com/protocolbuffers/protobuf/security/advisories)
* [Crypto++](https://www.cryptopp.com/)
* [Lua Bugs & Vulnerabilities](https://www.lua.org/bugs.html)
* [TinyXML (CVE Search)](https://cve.mitre.org/cgi-bin/cvekey.cgi?keyword=TinyXML)
* [MiniZ](https://github.com/richgel999/miniz)
