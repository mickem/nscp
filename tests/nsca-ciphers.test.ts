/**
 * Port of tests/nsca/run-test.bat. Brings up the nsca_server container
 * once per encryption method, submits one passive check via `nscp nsca`,
 * and verifies the result lands in the bind-mounted results.txt.
 *
 * Excluded ciphers (matching the bat file):
 *   * 3way (7)  - libmcrypt and nscp disagree
 *   * gost (23) - libmcrypt and nscp disagree
 *
 * AES variant note: libmcrypt's "rijndael 128" uses a non-standard key
 * size, so the matrix maps aes256 (nscp side) onto rijndael 128 (server
 * side / index 14). Cipher indexes 15 and 16 are not supported by nscp.
 */
import * as path from "path";
import {
  GenericContainer,
  NscpInstance,
  Wait,
  anyFileContains,
  containerChmodReadable,
  trackContainerLogs,
  type StartedTestContainer,
} from "@fixtures/index";

jest.setTimeout(600_000);

const PASSWORD = "change_me";

/**
 * [encryption-name-on-nscp-side, libmcrypt cipher index]
 *
 * `none` (0) and `xor` (1) work end-to-end on Linux.
 *
 * Ciphers 2+ submit successfully from nscp's side (the CLI prints
 * "Submission successful") but the server-side NSCA daemon decodes the
 * packet into garbage and never writes it to results.txt. This is a
 * libmcrypt / nscp key-derivation interop bug, not a test-harness
 * problem. Same matrix as tests/nsca/run-test.bat — if anyone fixes
 * the underlying issue, flip the `it.failing.each` below back to
 * `it.each` so a passing test is no longer flagged red.
 */
const CIPHERS: ReadonlyArray<readonly [string, number]> = [
  ["none",     0],
  ["xor",      1],
  ["des",      2],
  ["3des",     3],
  ["cast128",  4],
  ["xtea",     6],
  ["blowfish", 8],
  ["twofish",  9],
  ["rc2",      11],
  ["aes256",   14],
  ["serpent",  20],
];

describe("NSCA integration", () => {
  let image: GenericContainer;
  let nscp: NscpInstance;

  beforeAll(async () => {
    image = await GenericContainer.fromDockerfile(path.resolve(__dirname), "Dockerfiles/nsca.Dockerfile").build(
      "nsca_server",
      { deleteOnExit: false },
    );
    nscp = new NscpInstance();
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  async function submitCipher(cipher: string, idx: number): Promise<void> {
      const command = `encryption-${cipher}-${idx}`;
      // Each parametrized iteration gets its own bind-mount dir so a
      // previous test's results.txt can't confuse NSCA on container
      // start, and so an assertion failure on one cipher gives an
      // unambiguous spool to grep.
      const spoolDir = nscp.scratch(`nsca_test_${cipher}_${idx}`);
      let server: StartedTestContainer | undefined;
      try {
        // The bat file only sets ENCRYPTION_METHOD in docker env, but
        // on Linux that leaves the server-side nsca.cfg without a
        // password line and only the `none` cipher round-trips. Setting
        // PASSWORD as well writes `password=change_me` into nsca.cfg
        // (see entrypoint.sh) which is needed for `xor` to work too.
        // Ciphers 2+ still fail due to a libmcrypt / nscp key-derivation
        // mismatch — see KNOWN_BROKEN_CIPHERS above.
        server = await trackContainerLogs(
          await image
            .withExposedPorts(5667)
            .withEnvironment({
              ENCRYPTION_METHOD: String(idx),
              PASSWORD,
            })
            .withBindMounts([{ source: spoolDir, target: "/nsca", mode: "rw" }])
            .withWaitStrategy(Wait.forListeningPorts())
            .start(),
          `nsca_server_${cipher}_${idx}`,
        );
        const port = server.getMappedPort(5667);
        // The bat file slept 3s after `docker run` because nsca writes
        // the cfg/perm setup *before* it forks the daemon; Wait.forListeningPorts
        // can return as soon as the kernel queues SYNs, before the
        // daemon is actually accepting. forLogMessage(/NSCA is running/)
        // would also work but takes longer than the sleep on average.
        // NSCA's startup is non-deterministic: the daemon writes its
        // cfg/perm setup and then forks, and the kernel may accept TCP
        // connections briefly before the daemon's accept() loop is
        // wired up. Retry the submission a few times if the result
        // file hasn't gained the expected line yet — the cost is small
        // and it's a robust signal.
        let landed = false;
        for (let attempt = 0; attempt < 4 && !landed; attempt++) {
          await new Promise((res) => setTimeout(res, 1500));
          const r = await nscp.run([
            "nsca",
            "--host=127.0.0.1",
            `--port=${port}`,
            `--password=${PASSWORD}`,
            "--encryption", cipher,
            "--command", command,
            "--result", "2",
          ]);
          expect(r.exitCode).toBe(0);
          await containerChmodReadable(server, "/nsca");
          // Force the daemon to flush by closing its socket; the file
          // is mode-0660 but containerChmodReadable already promoted it.
          await new Promise((res) => setTimeout(res, 500));
          landed = anyFileContains(spoolDir, `${command};2`);
        }
        await server.stop();
        server = undefined;
        expect(landed).toBe(true);
      } finally {
        await server?.stop();
      }
  }

  it.each(CIPHERS)(
    "submits a CRIT result through encryption=%s (idx %i)",
    async (cipher, idx) => { await submitCipher(cipher, idx); },
  );
});
