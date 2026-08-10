/**
 * X-HTTP-Method-Override handling.
 *
 * The mongoose backend matched the header with
 * `strncmp(name, "X-HTTP-Method-Override", name.len)` — bounded by the
 * *incoming* header's length, so any name that is a prefix of it ("X",
 * "X-H", "X-HTTP", ...) silently changed the request method. That defeats
 * any upstream proxy or WAF ACL that classifies requests by method.
 *
 * /api/v2/login is the observable pair: GET reports the session, DELETE
 * revokes the token. So "did the method change" is answerable by asking
 * whether the token survived.
 */
import https from "node:https";
import request from "supertest";
import { NscpInstance, REST_URL, setupRestNscp } from "@fixtures/index";

/**
 * Issue a GET with raw headers, so a field can be sent more than once.
 * supertest's .set() replaces rather than appends, so it cannot express a
 * repeated header at all; Node emits one header line per array element.
 * Returns the status code.
 */
function rawGet(path: string, headers: Record<string, string | string[]>): Promise<number> {
  return new Promise((resolve, reject) => {
    const req = https.request(
      { host: "127.0.0.1", port: 8443, path, method: "GET", headers, rejectUnauthorized: false },
      (res) => {
        res.resume();
        res.on("end", () => resolve(res.statusCode ?? 0));
        res.on("error", reject);
      },
    );
    req.on("error", reject);
    req.end();
  });
}

jest.setTimeout(900_000);

describe("REST X-HTTP-Method-Override", () => {
  let nscp: NscpInstance;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await setupRestNscp(nscp);
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  /** Log in with Basic auth and return a fresh session token. */
  async function login(): Promise<string> {
    const response = await request(REST_URL)
      .get("/api/v2/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200);
    expect(response.body.key).toBeDefined();
    return response.body.key as string;
  }

  /** True when the token still authenticates. */
  async function tokenStillValid(token: string): Promise<boolean> {
    const response = await request(REST_URL).get("/api/v2/info").set("X-Auth-Token", token).trustLocalhost(true);
    return response.status === 200;
  }

  it("honours the full header name", async () => {
    const token = await login();
    await request(REST_URL)
      .get("/api/v2/login")
      .set("X-Auth-Token", token)
      .set("X-HTTP-Method-Override", "DELETE")
      .trustLocalhost(true)
      .expect(200);
    // The GET was dispatched as a DELETE, so the token is revoked.
    expect(await tokenStillValid(token)).toBe(false);
  });

  it("matches the header name case-insensitively", async () => {
    // RFC 7230 §3.2: field names are case-insensitive.
    const token = await login();
    await request(REST_URL)
      .get("/api/v2/login")
      .set("X-Auth-Token", token)
      .set("x-http-method-override", "DELETE")
      .trustLocalhost(true)
      .expect(200);
    expect(await tokenStillValid(token)).toBe(false);
  });

  it("ignores header names that are merely a prefix of it", async () => {
    // The regression: "X", "X-H" and "X-HTTP" all used to override the method.
    for (const header of ["X", "X-H", "X-HTTP", "X-HTTP-Method"]) {
      const token = await login();
      await request(REST_URL)
        .get("/api/v2/login")
        .set("X-Auth-Token", token)
        .set(header, "DELETE")
        .trustLocalhost(true)
        .expect(200);
      expect(await tokenStillValid(token)).toBe(true);
    }
  });

  it("ignores the override entirely when it is sent more than once", async () => {
    // Ambiguous: an upstream proxy or WAF that classifies by method has to
    // agree with the backend on which copy counts. Rather than pick a winner
    // (and pick a different one on each backend), both drop it.
    const token = await login();
    const status = await rawGet("/api/v2/login", {
      "X-Auth-Token": token,
      "X-HTTP-Method-Override": ["DELETE", "GET"],
    });
    expect(status).toBe(200);
    expect(await tokenStillValid(token)).toBe(true);
  });

  it("ignores a header name that merely starts with it", async () => {
    const token = await login();
    await request(REST_URL)
      .get("/api/v2/login")
      .set("X-Auth-Token", token)
      .set("X-HTTP-Method-Override-Extra", "DELETE")
      .trustLocalhost(true)
      .expect(200);
    expect(await tokenStillValid(token)).toBe(true);
  });
});
