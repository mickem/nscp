/**
 * Browser hardening headers on every REST answer.
 *
 * The agent serves an admin UI that holds a session token, and until now every
 * response carried only Content-Type. A page able to frame the UI therefore got
 * an authenticated one to click-jack, and any future injection had nothing
 * standing in its way. The headers are applied where the response is written,
 * so they cover the static SPA, the API and the error pages alike.
 *
 * Also pins the other half of the same change: the session credential is no
 * longer echoed back as a Set-Cookie nothing reads.
 */
import request from "supertest";
import { NscpInstance, REST_URL, setupRestNscp } from "@fixtures/index";

jest.setTimeout(900_000);

const expectHardened = (headers: Record<string, string | undefined>) => {
  expect(headers["x-frame-options"]).toEqual("DENY");
  expect(headers["x-content-type-options"]).toEqual("nosniff");
  expect(headers["referrer-policy"]).toEqual("no-referrer");
  expect(headers["content-security-policy"]).toBeDefined();
  expect(headers["content-security-policy"]).toContain("frame-ancestors 'none'");
};

describe("REST security headers", () => {
  let nscp: NscpInstance;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await setupRestNscp(nscp);
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("serves the UI with the hardening headers", async () => {
    const response = await request(REST_URL).get("/").trustLocalhost(true).expect(200);
    expectHardened(response.headers);
    // Served over TLS by the fixture, so HSTS is meaningful and emitted.
    expect(response.headers["strict-transport-security"]).toBeDefined();
  });

  it("puts them on an authenticated API response too", async () => {
    const response = await request(REST_URL)
      .get("/api/v2/info")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200);
    expectHardened(response.headers);
  });

  it("puts them on a rejected request, which is the one a browser is most likely to see", async () => {
    const response = await request(REST_URL).get("/api/v2/settings").trustLocalhost(true).expect(403);
    expectHardened(response.headers);
  });

  it("does not hand the session credential back as a cookie", async () => {
    // The server used to set the bearer as an HttpOnly `token` cookie (and the
    // user as `uid`) on every authenticated response. No request path ever
    // authenticated from a Cookie header, so the credential was simply stored
    // twice.
    const response = await request(REST_URL)
      .get("/api/v2/info")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200);
    const cookies = ([] as string[]).concat(response.headers["set-cookie"] ?? []);
    expect(cookies.filter((c) => c.startsWith("token=") || c.startsWith("uid="))).toEqual([]);
  });
});
