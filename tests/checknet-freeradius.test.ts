/**
 * check_radius against a REAL FreeRADIUS server, not our own fixture: proves
 * the PAP password hiding, the request Message-Authenticator and the reply
 * verification interoperate with an independent RADIUS implementation.
 * checknet-radius.test.ts covers the protocol edge cases with a Node fixture;
 * this suite is only about agreeing with a real server.
 *
 * Topology:
 *   nscp client (host process) --UDP 127.0.0.1:<mapped>--> FreeRADIUS :1812
 *
 * FreeRADIUS is configured by tests/Dockerfiles/entrypoints/freeradius/: one
 * client for every IPv4 source with `require_message_authenticator = yes`,
 * one PAP user, and Status-Server answered on the auth port. Its UDP port is
 * published with a raw `docker run -p ...:1812/udp` because testcontainers
 * only maps TCP.
 */
import * as fs from "node:fs";
import * as path from "node:path";
import execa from "execa";
import { NscpInstance, dockerOrSkip } from "@fixtures/index";

jest.setTimeout(600_000);

const IMAGE_TAG = "nscp_freeradius_it";
// Must match Dockerfiles/entrypoints/freeradius/{clients.conf,authorize}.
const secret = "radius-test-secret";
const password = "a password longer than sixteen bytes";

async function docker(args: string[], opts: { reject?: boolean } = {}) {
  return execa("docker", args, { reject: opts.reject ?? true, all: true });
}

dockerOrSkip()("CheckNet RADIUS against FreeRADIUS", () => {
  let containerId = "";
  let port = 0;
  let nscp: NscpInstance;
  let dir: string;
  let secretFile: string;
  let passwordFile: string;

  const logs = async () => {
    const r = await docker(["logs", containerId], { reject: false });
    return `${r.stdout}\n${r.stderr}`;
  };

  beforeAll(async () => {
    await execa(
      "docker",
      ["build", "-t", IMAGE_TAG, "-f", "Dockerfiles/freeradius.Dockerfile", "."],
      { cwd: path.resolve(__dirname), all: true },
    );
    const run = await docker(["run", "-d", "-p", "127.0.0.1::1812/udp", IMAGE_TAG]);
    containerId = run.stdout.trim();

    const deadline = Date.now() + 60_000;
    let ready = false;
    while (Date.now() < deadline && !ready) {
      ready = /Ready to process requests/.test(await logs());
      if (!ready) await new Promise((r) => setTimeout(r, 500));
    }
    if (!ready) throw new Error(`FreeRADIUS did not become ready. Logs:\n${await logs()}`);

    const portOut = (await docker(["port", containerId, "1812/udp"])).stdout.trim();
    port = Number(portOut.split("\n")[0].split(":").pop());
    if (!Number.isInteger(port) || port <= 0) {
      throw new Error(`could not parse mapped UDP port from: ${portOut}`);
    }

    nscp = new NscpInstance();
    dir = nscp.scratch("freeradius");
    secretFile = path.join(dir, "secret.txt");
    passwordFile = path.join(dir, "password.txt");
    fs.writeFileSync(secretFile, secret + "\n", { mode: 0o600 });
    fs.writeFileSync(passwordFile, password + "\n", { mode: 0o600 });
  });

  afterAll(async () => {
    if (containerId) await docker(["rm", "-f", containerId], { reject: false });
  });

  /**
   * One check_radius run. The timeout leaves room for FreeRADIUS's default
   * one-second reject_delay, which it applies to every Access-Reject.
   */
  async function query(args: string[], secretPath = secretFile, timeoutMs = 5000) {
    const result = await nscp.run(
      [
        "client",
        "--module",
        "CheckNet",
        "--boot",
        "--query",
        "check_radius",
        "host=127.0.0.1",
        `port=${port}`,
        `secret-file=${secretPath}`,
        `timeout=${timeoutMs}`,
        ...args,
      ],
      { allowFailure: true },
    );
    const output = result.all ?? `${result.stdout}\n${result.stderr}`;
    expect(output).not.toContain(secret);
    expect(output).not.toContain(password);
    return output;
  }

  it("authenticates a PAP user and gets a signed Access-Accept", async () => {
    const output = await query(["username=test-user", `password-file=${passwordFile}`]);
    expect(output).toMatch(/OK:.*reply=access_accept/);
    expect(output).toMatch(/_time'=\d+ms(?:\s|$)/);
  });

  it("reports a wrong password as a failed check, not as reachable", async () => {
    const wrong = path.join(dir, "wrong-password.txt");
    fs.writeFileSync(wrong, "not the password\n", { mode: 0o600 });
    const output = await query(["username=test-user", `password-file=${wrong}`]);
    expect(output).toMatch(/CRITICAL:.*unexpected_response.*reply=access_reject/);
  });

  it("reject mode expects the Access-Reject for a fictional user", async () => {
    const output = await query(["mode=reject"]);
    expect(output).toMatch(/OK:.*reply=access_reject/);
  });

  it("status mode gets an answer to an authenticated Status-Server", async () => {
    expect(await query(["mode=status"])).toMatch(/OK:.*reply=access_accept/);
  });

  it("times out when the shared secret is wrong", async () => {
    // FreeRADIUS silently drops a request whose Message-Authenticator does
    // not verify; it never answers, so check_radius sees a timeout.
    const wrong = path.join(dir, "wrong-secret.txt");
    fs.writeFileSync(wrong, "not-the-secret\n", { mode: 0o600 });
    const output = await query(["mode=status"], wrong, 1500);
    expect(output).toMatch(/CRITICAL:.*timeout, reply=none/);
  });
});
