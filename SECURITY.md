# Security Policy

## Reporting a Vulnerability

To report a vulnerability either use the [GitHub security vulnerability tool](https://github.com/mickem/nscp/security/advisories/new) or send an e-mail to the author michael at medin dot name.
Please note that due to spam filtering if you do not receive a reply to the email in 48 hours please re-try or preferably use the GitHub link.

## Which version should I use?

It is strongly recommended to use the latest release.
Most security issues are fixed in "the next version" unless they are severe.

## Pre-release versus release

We frequently publish pre-releases. It is in general safe to use those versions and they are not experimental in terms of untested code (review the release note which usually explains the changes). 
With a tool like NSClient++ which often goes deep it can sometimes be challanging to validate that everything works on everything and sometimes I want feedback on new features.

## Updates

There is no fixed cadence for updates but in general there are new updates about one per months. Sometimes there are more and sometimes there are less.
If you want to know dependencies for releases you can check the pipeline configuration to see versions: https://github.com/mickem/nscp/blob/main/.github/workflows/build-windows.yml#L30-L39 or you can review pipeline output which prints the same.
We try to ensure dependencies are reviewd and updated at least every 4 mounths and we also try to stay on top of things and capture security issues on a daily basis but if you find a security issue in a dependency please di let us know so we can review if it impacts NSClient.

Dependencies:
* [OpenSSL](https://openssl-library.org/news/vulnerabilities/index.html)
* [Mongoose](https://github.com/cesanta/mongoose/releases)
* [Boost](https://www.boost.org/releases/latest/)
* [Python](https://mail.python.org/archives/list/security-announce@python.org/latest)
* [Protobuf](https://github.com/protocolbuffers/protobuf/security/advisories)
* [Crypto++](https://www.cryptopp.com/)
* Lua - TODO
* Tiny XML - TODO
* MiniZ - TODO

