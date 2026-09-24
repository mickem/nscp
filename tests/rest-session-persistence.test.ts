/**
 * Web sessions survive a restart of the agent.
 *
 * Session tokens used to live only in the WEBServer module's memory, so
 * restarting the agent logged every web user out. They are now written to the
 * core storage (`${data-path}/nsclient.db`) — hashed, and bound to the
 * credentials they were issued against, so a logout or a password/role change
 * still invalidates them.
 *
 * The suite is in two halves.
 *
 * The first runs everywhere. It covers what does not need the agent to be
 * stopped cleanly: issuing keys, a logout revoking one on the spot, a settings
 * reload leaving keys alone, and — the one that pins the whole persistence
 * path end to end — a logout surviving a SIGKILL. The last one works because a
 * revocation writes the remaining table to disk immediately instead of waiting
 * for the shutdown save: the key that was logged out does not come back, and
 * the key that was not does, which can only be true if the file was rewritten
 * at the logout.
 *
 * The second half restarts the agent cleanly and is skipped on Windows, where
 * the fixture's stop() is a SIGKILL (there is no signal a Node parent can send
 * a Windows child that nscp turns into a clean shutdown), so the agent never
 * reaches the shutdown path that writes nsclient.db.
 *
 * What the two halves pin:
 *   a) a key issued before a clean stop still authenticates after the restart
 *   b) a key that was logged out (DELETE /api/v2/login) does not — neither
 *      after a clean restart nor after the process was killed outright
 *   c) a key whose user's password changed while the agent was down does not
 *   d) a settings reload (POST /api/v2/settings/command) keeps keys valid
 *   e) a user configured with a PLAINTEXT password has to log in again after a
 *      restart — the documented limitation, because user_manager re-salts a
 *      plaintext password on every boot and the credential fingerprint the
 *      session is bound to therefore changes
 *   f) an `nscp` CLI run while the service is stopped (`nscp settings`,
 *      `nscp web add-user` — the upgrade note tells operators to run the
 *      latter) leaves the saved sessions alone: the CLI loads and unloads
 *      the module too, and an unguarded shutdown export from its empty
 *      table would blank the row
 *   g) `persist sessions = false` restores the old behaviour: a restart ends
 *      every session, which is the documented way to invalidate them all
 *
 * The C++ side is platform neutral and is covered by the WEBServer_test unit
 * tests on every platform.
 *
 * The `persistent` user's password is stored pre-hashed, which is what
 * `nscp web install` writes for `admin` and the only form whose fingerprint is
 * stable across processes. The two literals below were produced with:
 *
 *   python3 -c "import hashlib,binascii
 *   s=binascii.unhexlify('000102030405060708090a0b0c0d0e0f')
 *   print('pbkdf2-sha256\$100000\$'+s.hex()+'\$'+
 *         hashlib.pbkdf2_hmac('sha256',b'persistent-password',s,100000,32).hex())"
 *
 * i.e. PBKDF2-HMAC-SHA256, 100000 iterations, 16-byte salt, 32-byte output,
 * formatted as password_hash.cpp writes it.
 */
import request from "supertest";
import { NscpInstance, REST_URL, setupRestNscp } from "@fixtures/index";

jest.setTimeout(900_000);

/** PBKDF2 form of "persistent-password" (see the header comment). */
const HASHED_PASSWORD =
  "pbkdf2-sha256$100000$000102030405060708090a0b0c0d0e0f$76576a36d586f4591939d6e1a842c8af99e53ec4a711f1d997f55a9b72298fb0";
/** PBKDF2 form of "rotated-password" — the same user, after the change. */
const HASHED_PASSWORD_ROTATED =
  "pbkdf2-sha256$100000$101112131415161718191a1b1c1d1e1f$e171b8232960a553000e07e32ea94c9bdb60882dbf601799472c93d7a9ec19c4";

const USER_PATH = "/settings/WEB/server/users/persistent";

/** A clean stop is a SIGTERM the agent turns into a shutdown; Windows has none. */
const describeCleanRestart = process.platform === "win32" ? describe.skip : describe;

describe("REST session persistence", () => {
  let nscp: NscpInstance;

  // The keys are threaded through the cases in order: a restart is expensive,
  // so the suite does a handful of them rather than one per assertion.
  let survivingKey = "";
  let plaintextKey = "";
  let secondKey = "";
  let rotatedKey = "";

  /** Log in with Basic auth and return the issued session key. */
  async function login(user: string, password: string): Promise<string> {
    const response = await request(REST_URL)
      .get("/api/v2/login")
      .auth(user, password)
      .trustLocalhost(true)
      .expect(200);
    expect(response.body.user).toEqual(user);
    expect(response.body.key).toBeTruthy();
    return response.body.key as string;
  }

  /** The status GET /api/v2/login answers with for a bearer key. */
  async function statusFor(key: string): Promise<number> {
    const response = await request(REST_URL)
      .get("/api/v2/login")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true);
    return response.status;
  }

  /** Log a key out the way the web UI's log out button does. */
  async function logout(key: string): Promise<void> {
    await request(REST_URL)
      .delete("/api/v2/login")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
  }

  /**
   * Poll until the server answers a request at all. A settings reload tears
   * the listener down and brings it back, and a request that lands in
   * between is refused rather than answered; that is the reload, not the
   * session.
   */
  async function waitForAnswer(probe: () => Promise<number>, timeoutMs: number): Promise<number> {
    const deadline = Date.now() + timeoutMs;
    let lastError: unknown;
    while (Date.now() < deadline) {
      try {
        return await probe();
      } catch (error) {
        lastError = error;
        await new Promise((r) => setTimeout(r, 250));
      }
    }
    throw new Error(`server did not answer within ${timeoutMs} ms: ${String(lastError)}`);
  }

  /** Start the agent again on the same work dir and wait for the listener. */
  async function start(): Promise<void> {
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
  }

  /**
   * Stop the agent the way a service stop does (SIGTERM), then start it again
   * so it reads back the nsclient.db it just wrote. The generous stop timeout
   * matters: a SIGKILL would skip the storage save and the suite would be
   * testing nothing.
   */
  async function restart(): Promise<void> {
    await nscp.stop({ timeout: 30_000 });
    await nscp.waitForPortFree(8443, { timeoutMs: 30_000 });
    await start();
  }

  /** Kill the agent outright, so nothing runs at shutdown, and start it again. */
  async function killAndRestart(): Promise<void> {
    await nscp.stop({ signal: "SIGKILL", timeout: 30_000 });
    await nscp.waitForPortFree(8443, { timeoutMs: 30_000 });
    await start();
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    // Applied before setupRestNscp so everything lands in the INI in one go,
    // while the agent is not running.
    await nscp.configure({
      [USER_PATH]: { role: "full", password: HASHED_PASSWORD },
      "/settings/WEB/server/users/plaintext": {
        role: "full",
        password: "plaintext-password",
      },
      // Migrated to a hash by `nscp web add-user` in the CLI case below.
      "/settings/WEB/server/users/migrate": {
        role: "full",
        password: "migrate-password",
      },
    });
    await setupRestNscp(nscp);
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("issues keys for a hashed-password and a plaintext-password user", async () => {
    survivingKey = await login("persistent", "persistent-password");
    plaintextKey = await login("plaintext", "plaintext-password");
    expect(await statusFor(survivingKey)).toEqual(200);
    expect(await statusFor(plaintextKey)).toEqual(200);
  });

  it("revokes a key the moment it is logged out", async () => {
    const throwaway = await login("persistent", "persistent-password");
    expect(await statusFor(throwaway)).toEqual(200);
    await logout(throwaway);
    expect(await statusFor(throwaway)).toEqual(403);
    // The other sessions of the same user are not collateral damage.
    expect(await statusFor(survivingKey)).toEqual(200);
  });

  it("keeps keys valid across a settings reload", async () => {
    secondKey = await login("persistent", "persistent-password");
    await request(REST_URL)
      .post("/api/v2/settings/command")
      .set("Authorization", `Bearer ${secondKey}`)
      .send({ command: "reload" })
      .trustLocalhost(true)
      .expect(200);
    // The reload is scheduled, not immediate, and it recreates the listener:
    // give it a couple of seconds, tolerate the refused connections while the
    // socket is down, and assert the key is still valid once it is back.
    await new Promise((r) => setTimeout(r, 2_000));
    expect(await waitForAnswer(() => statusFor(secondKey), 30_000)).toEqual(200);
    expect(await statusFor(survivingKey)).toEqual(200);
  });

  it("does not bring back a key that was logged out before the process was killed", async () => {
    // The revocation has to reach the disk when it happens, not at the next
    // clean shutdown - otherwise a crash (or a `kill -9`, or a power loss)
    // between the logout and the shutdown resurrects the session.
    const doomedKey = await login("persistent", "persistent-password");
    const keptKey = await login("persistent", "persistent-password");
    await logout(doomedKey);

    await killAndRestart();

    expect(await statusFor(doomedKey)).toEqual(403);
    // `keptKey` was issued after the last clean shutdown, so the only way it
    // can still work is if the logout above rewrote the stored table - which
    // is the same write that dropped `doomedKey` from it.
    expect(await statusFor(keptKey)).toEqual(200);
    expect(await statusFor(survivingKey)).toEqual(200);
  });

  describeCleanRestart("across a clean restart", () => {
    it("keeps a hashed-password user's key valid", async () => {
      await restart();
      expect(await statusFor(survivingKey)).toEqual(200);
    });

    it("keeps a key valid when the CLI ran while the service was stopped", async () => {
      // A CLI invocation boots the WEBServer module with dontStart, unloads it,
      // and saves nsclient.db - the same path a service stop takes. Its session
      // table is empty; writing that back would wipe the running service's
      // sessions. This is the `nscp web add-user` an operator is told to run to
      // migrate a cleartext password, plus a plain `nscp settings`.
      await nscp.stop({ timeout: 30_000 });
      await nscp.waitForPortFree(8443, { timeoutMs: 30_000 });
      await nscp.run([
        "settings",
        "--path",
        "/settings/WEB/server",
        "--key",
        "persist sessions",
        "--set",
        "true",
      ]);
      await nscp.run(["web", "add-user", "--user", "migrate", "--role", "full"]);
      await start();
      expect(await statusFor(survivingKey)).toEqual(200);
      // ... and the migration itself worked: the user still logs in with the
      // password they had.
      expect(await statusFor(await login("migrate", "migrate-password"))).toEqual(200);
    });

    it("does not carry a plaintext-password user's key over", async () => {
      // Documented limitation: a plaintext INI password is re-salted by
      // user_manager on every boot, so the credential fingerprint the session
      // was bound to no longer matches and the session is dropped on import.
      expect(await statusFor(plaintextKey)).toEqual(403);
      // ... and logging in again works, so this is a re-login, not a lockout.
      plaintextKey = await login("plaintext", "plaintext-password");
      expect(await statusFor(plaintextKey)).toEqual(200);
    });

    it("does not restore a key that was logged out before the stop", async () => {
      await logout(survivingKey);
      expect(await statusFor(survivingKey)).toEqual(403);
      await restart();
      expect(await statusFor(survivingKey)).toEqual(403);
      // The key that was NOT logged out still works, so the restart itself is
      // not what invalidated the revoked one.
      expect(await statusFor(secondKey)).toEqual(200);
    });

    it("does not restore a key after the user's password changed", async () => {
      await nscp.stop({ timeout: 30_000 });
      await nscp.waitForPortFree(8443, { timeoutMs: 30_000 });
      await nscp.run([
        "settings",
        "--path",
        USER_PATH,
        "--key",
        "password",
        "--set",
        HASHED_PASSWORD_ROTATED,
      ]);
      await start();

      expect(await statusFor(secondKey)).toEqual(403);
      // The new password works, so the user is usable - only the old sessions
      // are gone.
      rotatedKey = await login("persistent", "rotated-password");
      expect(await statusFor(rotatedKey)).toEqual(200);
    });

    it("ends every session when persist sessions is off", async () => {
      await nscp.stop({ timeout: 30_000 });
      await nscp.waitForPortFree(8443, { timeoutMs: 30_000 });
      await nscp.run([
        "settings",
        "--path",
        "/settings/WEB/server",
        "--key",
        "persist sessions",
        "--set",
        "false",
      ]);
      await start();
      // The row an earlier run wrote is ignored...
      expect(await statusFor(rotatedKey)).toEqual(403);
      const volatileKey = await login("persistent", "rotated-password");
      expect(await statusFor(volatileKey)).toEqual(200);

      // ... and blanked at this shutdown, so switching persistence back on does
      // not resurrect anything from before it was switched off.
      await nscp.stop({ timeout: 30_000 });
      await nscp.waitForPortFree(8443, { timeoutMs: 30_000 });
      await nscp.run([
        "settings",
        "--path",
        "/settings/WEB/server",
        "--key",
        "persist sessions",
        "--set",
        "true",
      ]);
      await start();
      expect(await statusFor(rotatedKey)).toEqual(403);
      expect(await statusFor(volatileKey)).toEqual(403);
      expect(await statusFor(await login("persistent", "rotated-password"))).toEqual(200);
    });
  });
});
