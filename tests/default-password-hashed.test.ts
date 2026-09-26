/**
 * What the readers of the shared /settings/default/password do when it holds
 * the hashed form (pbkdf2-sha256$...) that `nscp web install` and `nscp web
 * password --set` write:
 *
 *   - NSClientServer (check_nt) verifies the clear-text password the client
 *     sends against the hash, exactly as it verifies against a clear-text
 *     value, and the hash string itself is not a valid credential;
 *   - NSCAServer does not read that section at all any more. Its key is shared
 *     with the hosts submitting to this agent, it is inherited from nowhere,
 *     and the server refuses to start rather than listen without one.
 *
 * check_nt is spoken over a raw socket here (`<password>&<code>`, no framing,
 * one response then close), so this runs without docker; the real
 * nagios-plugins client is exercised in check_nt-client.test.ts. That half
 * needs NSClientServer, which is built on Windows only, so it is skipped where
 * the install does not carry the module; NSCAServer is cross-platform and its
 * half runs everywhere crypto++ was found.
 */
import * as crypto from "crypto";
import * as net from "net";
import { NscpInstance, hasModule } from "@fixtures/index";

jest.setTimeout(180_000);

const CHECK_NT_PORT = 12499;
const NSCA_PORT = 5677;
const PASSWORD = "check_nt-secret";

/** The stored form password_hash.cpp writes: pbkdf2-sha256, 16-byte salt, 32-byte hash. */
function pbkdf2Hash(password: string): string {
  const salt = crypto.randomBytes(16);
  const iterations = 100000;
  const hash = crypto.pbkdf2Sync(password, salt, iterations, 32, "sha256");
  return `pbkdf2-sha256$${iterations}$${salt.toString("hex")}$${hash.toString("hex")}`;
}

/** One check_nt request: send `request`, collect the reply until the server closes. */
function checkNt(request: string): Promise<string> {
  return new Promise((resolve, reject) => {
    let out = "";
    const s = net.createConnection({ host: "127.0.0.1", port: CHECK_NT_PORT }, () => {
      s.write(request);
    });
    s.setTimeout(15_000);
    s.on("data", (b: Buffer) => {
      out += b.toString();
    });
    s.on("close", () => resolve(out));
    s.on("timeout", () => {
      s.destroy();
      resolve(out);
    });
    s.on("error", reject);
  });
}

/** True when something accepts a TCP connection on `port`. */
function portOpen(port: number): Promise<boolean> {
  return new Promise((resolve) => {
    const s = new net.Socket();
    s.setTimeout(1000);
    s.once("error", () => {
      s.destroy();
      resolve(false);
    });
    s.once("timeout", () => {
      s.destroy();
      resolve(false);
    });
    s.connect(port, "127.0.0.1", () => {
      s.end();
      resolve(true);
    });
  });
}

/** Poll the agent's stdout until `pattern` shows up (or give up). */
async function waitForOutput(
  nscp: NscpInstance,
  pattern: RegExp,
  timeoutMs: number,
): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (pattern.test(nscp.capturedStdout())) return;
    await new Promise((r) => setTimeout(r, 250));
  }
  throw new Error(
    `nscp did not log ${pattern} within ${timeoutMs}ms:\n${nscp.capturedStdout()}\n${nscp.capturedStderr()}`,
  );
}

// NSClientServer is Windows-only (modules/NSClientServer/module.cmake sets
// BUILD_MODULE 0 elsewhere), so on a Linux package there is no check_nt server
// to configure and the whole group is skipped rather than left waiting on a
// port nothing will open. Asked of the install rather than of the platform: a
// build may omit a module the platform supports.
const describeCheckNt = hasModule("NSClientServer") ? describe : describe.skip;

describeCheckNt("check_nt with the shared password stored hashed", () => {
  const stored = pbkdf2Hash(PASSWORD);
  let nscp: NscpInstance;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": { NSClientServer: "enabled" },
      "/settings/default": { "allowed hosts": "127.0.0.1", password: stored },
      "/settings/NSClient/server": { "use ssl": false, port: String(CHECK_NT_PORT) },
    });
    await nscp.waitForPortFree(CHECK_NT_PORT, { timeoutMs: 30_000 });
    nscp.start();
    await nscp.waitForPort(CHECK_NT_PORT, { timeoutMs: 30_000 });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("accepts the clear-text password the client sends", async () => {
    // Request code 1 is CLIENTVERSION, answered inline with the agent version.
    const out = await checkNt(`${PASSWORD}&1`);
    expect(out).toMatch(/\d+\.\d+\.\d+/);
    expect(out).not.toContain("ERROR");
  });

  it("rejects a wrong password with the generic error", async () => {
    const out = await checkNt("not-the-password&1");
    expect(out).toContain("ERROR: Bad request.");
  });

  it("does not accept the stored hash itself as the password", async () => {
    // Reading nsclient.ini now yields the hash, and the hash is not a credential.
    const out = await checkNt(`${stored}&1`);
    expect(out).toContain("ERROR: Bad request.");
  });
});

describe("NSCAServer and the key it will not inherit", () => {
  // NSCAServer is only built where crypto++ is available.
  const available = hasModule("NSCAServer");

  /** Boot with this config and hand the instance back, stopped by the caller. */
  async function bootServer(extra: Record<string, Record<string, unknown>>): Promise<NscpInstance> {
    const nscp = new NscpInstance();
    await nscp.configure({
      "/modules": { NSCAServer: "enabled" },
      "/settings/NSCA/server": { encryption: "xor", port: String(NSCA_PORT) },
      ...extra,
    });
    await nscp.waitForPortFree(NSCA_PORT, { timeoutMs: 30_000 });
    nscp.start();
    return nscp;
  }

  it("refuses to start with no key of its own, however the shared default is written", async () => {
    if (!available) return;
    // The shared section holds a perfectly good clear-text password here, and
    // it is still not the NSCA key: that one is shared with the hosts
    // submitting to this agent and has to be set on purpose.
    const nscp = await bootServer({
      "/settings/default": { "allowed hosts": "127.0.0.1", password: "not-the-nsca-key" },
    });
    try {
      await waitForOutput(nscp, /Refusing to start NSCA server: encryption is enabled/, 30_000);
      expect(nscp.capturedStdout()).toContain("/settings/NSCA/server");
      expect(await portOpen(NSCA_PORT)).toBe(false);
    } finally {
      await nscp.stop();
    }
  });

  it("does not borrow the key NSCAClient submits with", async () => {
    if (!available) return;
    // Two different peers, two different secrets: the client target's key is
    // what this agent sends to a remote daemon, and handing it to the listener
    // would quietly turn it into a credential for inbound submissions.
    const nscp = await bootServer({
      "/settings/NSCA/client/targets/default": {
        address: "nagios.example.com",
        password: "the-client-key",
        encryption: "aes256",
      },
    });
    try {
      await waitForOutput(nscp, /Refusing to start NSCA server: encryption is enabled/, 30_000);
      expect(await portOpen(NSCA_PORT)).toBe(false);
    } finally {
      await nscp.stop();
    }
  });

  it("refuses a key that is a stored hash rather than deriving from it", async () => {
    if (!available) return;
    // Reachable only by pasting one in, now that nothing is inherited - but a
    // hash is never a usable key, so it is still a refusal and not a deaf port.
    const nscp = await bootServer({
      "/settings/NSCA/server": {
        encryption: "xor",
        port: String(NSCA_PORT),
        password: pbkdf2Hash("nsca-secret"),
      },
    });
    try {
      await waitForOutput(nscp, /Refusing to start NSCA server: the password is stored hashed/, 30_000);
      expect(await portOpen(NSCA_PORT)).toBe(false);
    } finally {
      await nscp.stop();
    }
  });

  it("starts once the NSCA section carries a clear-text key of its own", async () => {
    if (!available) return;
    const nscp = await bootServer({
      "/settings/default": { "allowed hosts": "127.0.0.1", password: pbkdf2Hash("something-else") },
      "/settings/NSCA/server": {
        encryption: "xor",
        port: String(NSCA_PORT),
        password: "nsca-secret",
      },
    });
    try {
      await nscp.waitForPort(NSCA_PORT, { timeoutMs: 30_000 });
      expect(nscp.capturedStdout()).not.toContain("Refusing to start NSCA server");
    } finally {
      await nscp.stop();
    }
  });

  it("starts without a key when encryption is off", async () => {
    if (!available) return;
    // No cipher, no key to be missing: the refusal is about a well-known key,
    // not about the setting being unset.
    const nscp = await bootServer({
      "/settings/NSCA/server": { encryption: "none", port: String(NSCA_PORT) },
    });
    try {
      await nscp.waitForPort(NSCA_PORT, { timeoutMs: 30_000 });
      expect(nscp.capturedStdout()).not.toContain("Refusing to start NSCA server");
    } finally {
      await nscp.stop();
    }
  });
});
