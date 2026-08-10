/**
 * The legacy /auth/token endpoint must be subject to the same per-IP failed
 * authentication backoff as every other credential entry point.
 *
 * It used to call validate_user() directly, bypassing auth_rate_limiter
 * entirely, which made it an unthrottled password-guessing oracle against the
 * admin account - and it answered "403 Invalid password" only for a wrong
 * password, confirming when a guess was right. The User-Agent allowlist in
 * front of it is not a control: the client chooses its own User-Agent.
 *
 * This suite runs in its own NscpInstance because it deliberately gets the
 * source IP blocked, which would leak into any other scenario sharing the
 * server.
 */
import request from "supertest";
import { NscpInstance, REST_URL, setupRestNscp } from "@fixtures/index";

jest.setTimeout(900_000);

const ICINGA_UA = "Icinga/check_nscp_api/2.14.0";
const MAX_FAILURES = 3;
const BLOCK_SECONDS = 3;

const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

/** One /auth/token attempt with the correct password, without asserting a status. */
const attemptWithCorrectPassword = () =>
  request(REST_URL)
    .get("/auth/token")
    .set("User-Agent", ICINGA_UA)
    .query({ password: "default-password" })
    .trustLocalhost(true);

describe("REST legacy /auth/token — failed-auth throttling", () => {
  let nscp: NscpInstance;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await setupRestNscp(nscp);
    await nscp.stop();
    // Tighten the limiter so the test does not need ten round trips and a
    // 60 second wait. The defaults (10 failures / 60s) are what ships.
    await nscp.configure({
      "/settings/WEB/server": {
        "auth rate limit max failures": String(MAX_FAILURES),
        "auth rate limit block seconds": String(BLOCK_SECONDS),
      },
    });
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("blocks the caller after repeated wrong passwords, even with the right one", async () => {
    for (let i = 0; i < MAX_FAILURES; i++) {
      await request(REST_URL)
        .get("/auth/token")
        .set("User-Agent", ICINGA_UA)
        .query({ password: `wrong-password-${i}` })
        .trustLocalhost(true)
        .expect(403);
    }

    // The block is what proves the limiter is engaged: the correct password
    // is now refused too. Before the fix this returned 200 and a token.
    await request(REST_URL)
      .get("/auth/token")
      .set("User-Agent", ICINGA_UA)
      .query({ password: "default-password" })
      .trustLocalhost(true)
      .expect(403);
  });

  it("recovers once the block expires", async () => {
    // Poll rather than sleeping exactly one second past the block: a fixed
    // margin turns scheduler delays and timer granularity on a loaded CI box
    // into a spurious failure. Give it several times the block duration to
    // recover, but stop the moment it does.
    const deadline = Date.now() + (BLOCK_SECONDS + 10) * 1000;
    let response = await attemptWithCorrectPassword();
    while (response.status === 403 && Date.now() < deadline) {
      await sleep(250);
      response = await attemptWithCorrectPassword();
    }

    expect(response.status).toBe(200);
    expect(response.headers["content-type"]).toMatch(/application\/json/);
    expect(response.body.status).toEqual("ok");
    // Not toBeDefined(): an empty string is "defined" and would sail through,
    // which is exactly the regression worth catching - the endpoint returns
    // "ok" plus a token it read back out of the response, so a token that
    // never got stored shows up here as "".
    expect(typeof response.body["auth token"]).toBe("string");
    expect(response.body["auth token"].length).toBeGreaterThan(0);
    expect(response.headers.__token).toEqual(response.body["auth token"]);
  });

  it("does not distinguish a wrong password from an unknown outcome", async () => {
    // A single generic 403 for every failure mode. The old endpoint replied
    // "403 Invalid password" specifically for a bad password, which is a clean
    // password-correctness oracle.
    const failure = await request(REST_URL)
      .get("/auth/token")
      .set("User-Agent", ICINGA_UA)
      .query({ password: "definitely-not-the-password" })
      .trustLocalhost(true)
      .expect(403);

    expect(failure.text).not.toMatch(/invalid password/i);
  });
});
