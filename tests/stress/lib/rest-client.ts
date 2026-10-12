import { Pool } from "undici";

import { REST_URL } from "@fixtures/rest-fixture";

/**
 * The HTTP client the load workers share: an undici pool with one connection
 * per worker and a bearer token from one login. The agent's beast backend
 * answers every request with `Connection: close`, so each request is a fresh
 * TLS connection whichever client makes it - the accept loop, the handshake
 * and the session coroutine are part of what a flood exercises - and the pool
 * is here for its bounded concurrency and its low per-request overhead, not
 * for keep-alive.
 */
export class RestClient {
  private readonly pool: Pool;
  private key = "";

  constructor(connections: number, url: string = REST_URL) {
    this.pool = new Pool(url, {
      connections,
      pipelining: 1,
      keepAliveTimeout: 30_000,
      // The agent's certificate is self-signed and generated per test.
      connect: { rejectUnauthorized: false },
    });
  }

  /** Log in with basic auth; later calls carry the returned bearer key. */
  async login(user = "admin", password = "default-password"): Promise<void> {
    const res = await this.pool.request({
      method: "GET",
      path: "/api/v1/login",
      headers: { authorization: `Basic ${Buffer.from(`${user}:${password}`).toString("base64")}` },
    });
    const body = (await res.body.json()) as { key?: string };
    if (res.statusCode !== 200 || !body.key) {
      throw new Error(`login failed: HTTP ${res.statusCode} ${JSON.stringify(body)}`);
    }
    this.key = body.key;
  }

  /** GET `path`; throws on a non-200 answer. Returns the body text. */
  async get(path: string): Promise<string> {
    const { text } = await this.getRaw(path);
    return text;
  }

  private async getRaw(
    path: string,
  ): Promise<{ text: string; headers: Record<string, string | string[] | undefined> }> {
    const res = await this.pool.request({
      method: "GET",
      path,
      headers: { authorization: `Bearer ${this.key}` },
    });
    const text = await res.body.text();
    if (res.statusCode !== 200) {
      throw new Error(`GET ${path.split("?")[0]} -> HTTP ${res.statusCode}: ${text.slice(0, 200)}`);
    }
    return { text, headers: res.headers };
  }

  /** GET `path` and parse the body as JSON. */
  async getJson<T>(path: string): Promise<T> {
    const { text, headers } = await this.getRaw(path);
    try {
      return JSON.parse(text) as T;
    } catch (e) {
      // A 200 whose body is not JSON: say which route, how long the body was
      // and what the server declared, so an empty or truncated answer under
      // load is diagnosable from the report.
      throw new Error(
        `GET ${path.split("?")[0]} -> HTTP 200 but not JSON (${(e as Error).message}); ` +
          `body ${text.length} bytes, content-length ${String(headers["content-length"])}, ` +
          `content-type ${String(headers["content-type"])}, connection ${String(headers["connection"])}: ` +
          JSON.stringify(text.slice(0, 120)),
      );
    }
  }

  /** PUT a JSON body to `path`; throws on a non-200 answer. */
  async putJson(path: string, body: unknown): Promise<string> {
    const res = await this.pool.request({
      method: "PUT",
      path,
      headers: { authorization: `Bearer ${this.key}`, "content-type": "application/json" },
      body: JSON.stringify(body),
    });
    const text = await res.body.text();
    if (res.statusCode !== 200) {
      throw new Error(`PUT ${path} -> HTTP ${res.statusCode}: ${text.slice(0, 200)}`);
    }
    return text;
  }

  /** DELETE `path`; throws on a non-200 answer. */
  async delete(path: string): Promise<string> {
    const res = await this.pool.request({
      method: "DELETE",
      path,
      headers: { authorization: `Bearer ${this.key}` },
    });
    const text = await res.body.text();
    if (res.statusCode !== 200) {
      throw new Error(`DELETE ${path} -> HTTP ${res.statusCode}: ${text.slice(0, 200)}`);
    }
    return text;
  }

  /**
   * Run a query through /api/v1/queries/<command>/commands/execute and return
   * the parsed result. Throws when the HTTP status or the Nagios status is not
   * the one expected.
   */
  async query(
    command: string,
    args: Record<string, string> = {},
    expectStatus = 0,
  ): Promise<{ result: number; lines: { message: string }[] }> {
    const qs = new URLSearchParams(args).toString();
    const body = await this.getJson<{ result: number; lines: { message: string }[] }>(
      `/api/v1/queries/${command}/commands/execute${qs ? `?${qs}` : ""}`,
    );
    if (body.result !== expectStatus) {
      throw new Error(
        `${command} answered status ${body.result}, expected ${expectStatus}: ${body.lines?.[0]?.message ?? ""}`,
      );
    }
    return body;
  }

  async close(): Promise<void> {
    await this.pool.close();
  }
}
