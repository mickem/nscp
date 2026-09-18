/**
 * Web sessions survive a restart of the agent.
 *
 * Session tokens used to live only in the WEBServer module's memory, so
 * restarting the agent logged every web user out. They are now written to the
 * core storage (`${data-path}/nsclient.db`) at a clean shutdown and read back
 * at boot — hashed, and bound to the credentials they were issued against, so
 * a logout or a password/role change still invalidates them.
 *
 * What this suite pins:
 *   a) a key issued before a clean stop still authenticates after the restart
 *   b) a key that was logged out (DELETE /api/v2/login) does not
 *   c) a key whose user's password changed while the agent was down does not
 *   d) a settings reload (POST /api/v2/settings/command) keeps keys valid
 *   e) a user configured with a PLAINTEXT password has to log in again after a
 *      restart — the documented limitation, because user_manager re-salts a
 *      plaintext password on every boot and the credential fingerprint the
 *      session is bound to therefore changes
 *   f) the token a Basic-auth request mints in passing (every request does;
 *      it comes back as a `token` cookie) is never persisted — only the key
 *      the login endpoint hands out is, so a monitoring system polling with
 *      Basic auth does not fill nsclient.db
 *   g) `persist sessions = false` restores the old behaviour: a restart ends
 *      every session, which is the documented way to invalidate them all
 *
 * Windows is skipped: the fixture's stop() there is a SIGKILL (there is no
 * signal a Node parent can send a Windows child that nscp turns into a clean
 * shutdown), so the agent never reaches the shutdown path that writes
 * nsclient.db and every "survives a restart" case would fail for a reason
 * that has nothing to do with the code under test. The C++ side is platform
 * neutral and is covered by the WEBServer_test unit tests on every platform.
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

const notOnWindows = process.platform === "win32" ? describe.skip : describe;

notOnWindows("REST session persistence", () => {
  let nscp: NscpInstance;

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

  /**
   * The status GET /api/v2/login answers with for a bearer key. Note that
   * presenting a key to the login endpoint is what marks its session as held
   * by a client (that is the endpoint that hands keys out), so a token that
   * must stay volatile is probed through infoStatusFor() instead.
   */
  async function statusFor(key: string): Promise<number> {
    const response = await request(REST_URL)
      .get("/api/v2/login")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true);
    return response.status;
  }

  /** The status GET /api/v2/info answers with for a bearer key. */
  async function infoStatusFor(key: string): Promise<number> {
    const response = await request(REST_URL)
      .get("/api/v2/info")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true);
    return response.status;
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

  /**
   * Stop the agent the way a service stop does (SIGTERM), then start it again
   * on the same work dir so it reads back the nsclient.db it just wrote. The
   * generous stop timeout matters: a SIGKILL would skip the storage save and
   * the suite would be testing nothing.
   */
  async function restart(): Promise<void> {
    await nscp.stop({ timeout: 30_000 });
    await nscp.waitForPortFree(8443, { timeoutMs: 30_000 });
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
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
    });
    await setupRestNscp(nscp);
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  // The keys are threaded through the cases in order: a restart is expensive,
  // so the suite does three of them rather than one per assertion.
  let survivingKey = "";
  let plaintextKey = "";
  let secondKey = "";
  let inPassingToken = "";

  it("issues keys for a hashed-password and a plaintext-password user", async () => {
    survivingKey = await login("persistent", "persistent-password");
    plaintextKey = await login("plaintext", "plaintext-password");
    expect(await statusFor(survivingKey)).toEqual(200);
    expect(await statusFor(plaintextKey)).toEqual(200);
  });

  it("mints a volatile token for a Basic-auth request on any other route", async () => {
    // Every Basic-auth request creates a session and echoes its token as a
    // cookie; nothing but the login endpoint ever returns one to a client on
    // purpose. This one works for as long as the process lives...
    const response = await request(REST_URL)
      .get("/api/v2/info")
      .auth("persistent", "persistent-password")
      .trustLocalhost(true)
      .expect(200);
    const cookies = ([] as string[]).concat(response.headers["set-cookie"] ?? []);
    const match = cookies.map((c) => /^token=([A-Za-z0-9]+)/.exec(c)).find((m) => m);
    expect(match).toBeTruthy();
    inPassingToken = match![1];
    expect(await infoStatusFor(inPassingToken)).toEqual(200);
  });

  it("keeps a hashed-password user's key valid across a restart", async () => {
    await restart();
    expect(await statusFor(survivingKey)).toEqual(200);
  });

  it("does not carry a Basic-auth request's token across a restart", async () => {
    // ... but it was never handed out, so it was never persisted.
    expect(await infoStatusFor(inPassingToken)).toEqual(403);
  });

  it("does not carry a plaintext-password user's key across a restart", async () => {
    // Documented limitation: a plaintext INI password is re-salted by
    // user_manager on every boot, so the credential fingerprint the session
    // was bound to no longer matches and the session is dropped on import.
    expect(await statusFor(plaintextKey)).toEqual(403);
    // ... and logging in again works, so this is a re-login, not a lockout.
    plaintextKey = await login("plaintext", "plaintext-password");
    expect(await statusFor(plaintextKey)).toEqual(200);
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

  it("does not restore a key that was logged out before the stop", async () => {
    await request(REST_URL)
      .delete("/api/v2/login")
      .set("Authorization", `Bearer ${survivingKey}`)
      .trustLocalhost(true)
      .expect(200);
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
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });

    expect(await statusFor(secondKey)).toEqual(403);
    // The new password works, so the user is usable - only the old sessions
    // are gone.
    rotatedKey = await login("persistent", "rotated-password");
    expect(await statusFor(rotatedKey)).toEqual(200);
  });

  let rotatedKey = "";

  it("ends every session at a restart when persist sessions is off", async () => {
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
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
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
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
    expect(await statusFor(rotatedKey)).toEqual(403);
    expect(await statusFor(volatileKey)).toEqual(403);
    expect(await statusFor(await login("persistent", "rotated-password"))).toEqual(200);
  });
});
