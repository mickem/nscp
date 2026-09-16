# Mod-Gearman payload fixtures

Raw gearman job workloads captured from the two Mod-Gearman flavours, byte
for byte as they left the wire, plus the result payloads their `send_gearman`
tools produce. They are the reference the GearmanClient module's decoder and
encoder (and the TypeScript fixture in `tests/src/gearman.ts`) are tested
against: `tests/gearman-fixtures.test.ts` decrypts every file and proves that
re-encrypting the decrypted text reproduces the captured bytes exactly.

| File                                | Produced by                                                          |
| ----------------------------------- | -------------------------------------------------------------------- |
| `naemon-job-host.b64`               | ConSol `mod_gearman_naemon.o` on Naemon, host check for `nscp-test`  |
| `naemon-job-service.b64`            | same, service check `helper` (`command_line=check_ok message=hello`) |
| `naemon-result-passive-service.b64` | ConSol `send_gearman`, passive service result                        |
| `naemon-result-passive-host.b64`    | ConSol `send_gearman`, passive host result                           |
| `naemon-result-active-service.b64`  | ConSol `send_gearman --active` with fixed start/finish time          |
| `nagios-job-host.b64`               | `nagios-mod-gearman.o` on Nagios Core 4.5, host check                |
| `nagios-job-service.b64`            | same, service check `helper`                                         |
| `nagios-result-passive-service.b64` | `nagios-send-gearman`, passive service result                        |
| `nagios-result-passive-host.b64`    | `nagios-send-gearman`, passive host result                           |
| `nagios-result-active-service.b64`  | `nagios-send-gearman --active` with fixed start/finish time          |
| `*-versions.txt`                    | Exact versions that produced each set                                |
| `key.txt`                           | The shared password (`nscp-test-key`, no trailing newline)           |

Every payload is `base64(AES-256-ECB(key32, zero_pad16(text + NUL)))` with the
password NUL-padded to 32 bytes and OpenSSL's own padding disabled, as
`common/gm_crypt.c` does in both projects. The files carry no trailing
newline; they are the workload bytes exactly.

## What the captures established

- **The two flavours share the envelope.** Every file decrypts with the same
  routine, and the same routine re-encrypts the text back to the identical
  base64. There is no divergence in key handling, padding or encoding
  between ConSol mod_gearman 5.2.5 and nagios-mod-gearman 1.0.1.
- **The job text differs by two lines.** The Nagios fork writes
  `start_time=<epoch>.0` and `next_check=<epoch>.0` between the object names
  and `core_time`; Naemon's module does not. Everything else, including
  field order, is the same, so a decoder must treat every field after `type`
  as optional and ignore unknown keys.
- **The result text is the same** apart from `source=` (`send_gearman` versus
  `nagios-send-gearman`). Newlines inside `output` travel as the two
  characters `\n`.
- **Zero padding is applied to `strlen + 1`.** The terminating NUL is part
  of the plaintext, so a decoder cuts at the first NUL rather than stripping
  a known pad length. A plaintext whose length (with NUL) is a multiple of 16
  other than 16 gets a whole extra zero block, a quirk of the reference
  implementation's `BLOCKSIZE % len` test that the decoder never notices.

## Regenerating

`capture.sh` in this directory runs each core image in capture mode, where the
entrypoint (`tests/Dockerfiles/entrypoints/gearman-core.sh`) grabs the jobs the
NEB module schedules for `hostgroup_gearman-test` with the `gearman` CLI in
worker mode, classifies them by decrypting with the openssl CLI, and produces
the result payloads with `send_gearman` on a side queue:

```sh
modules/GearmanClient/fixtures/capture.sh            # both cores, via docker
modules/GearmanClient/fixtures/capture.sh nagios     # one of them
```

Without docker, the same entrypoint runs natively against a from-source
build; every path is an environment variable (see the top of the script):

```sh
GEARMAN_CORE=naemon CORE_BIN=/opt/naemon/bin/naemon \
  NEB_MODULE=/opt/mod_gearman/lib/mod_gearman/mod_gearman_naemon.o \
  SEND_GEARMAN=/opt/mod_gearman/bin/send_gearman \
  GEARMAND_LISTEN=127.0.0.1 WORK=/var/tmp/gearman-naemon \
  CAPTURE_DIR=$PWD/modules/GearmanClient/fixtures \
  tests/Dockerfiles/entrypoints/gearman-core.sh
```

The job payloads carry the capture time in `core_time` (and `start_time` /
`next_check` for Nagios), so regenerating changes those files; the
`*-result-active-service.b64` payloads use fixed times and are stable across
runs. After regenerating, run `tests/gearman-fixtures.test.ts` and update the
`*-versions.txt` files in the same commit.
