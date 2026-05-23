import execa from "execa";

/**
 * Plain HTTP/HTTPS helpers built on the system `curl` binary. We don't
 * use a node HTTP client because:
 *   - curl is everywhere already
 *   - self-signed certs + basic auth are trivially expressed
 *   - the original bat tests use curl and we want byte-for-byte parity
 *     on what hits the wire
 */

export interface CurlOptions {
  /** Basic-auth user:password. */
  auth?: { user: string; password: string };
  /** Skip TLS verification. Default true (Icinga has a self-signed cert). */
  insecure?: boolean;
  /** Timeout in ms. */
  timeoutMs?: number;
}

/** GET a URL; return the body as a string. Throws on HTTP error. */
export async function curlGet(url: string, opts: CurlOptions = {}): Promise<string> {
  const args = ["-s", "-f"];
  if (opts.insecure ?? true) args.push("-k");
  if (opts.auth) args.push("-u", `${opts.auth.user}:${opts.auth.password}`);
  args.push(url);
  const r = await execa("curl", args, { timeout: opts.timeoutMs ?? 15_000 });
  return r.stdout;
}

/** GET, returning only the HTTP status code (string, "000" on connect failure). */
export async function curlHead(url: string, opts: CurlOptions = {}): Promise<string> {
  const args = ["-s", "-o", "/dev/null", "-w", "%{http_code}"];
  if (opts.insecure ?? true) args.push("-k");
  if (opts.auth) args.push("-u", `${opts.auth.user}:${opts.auth.password}`);
  args.push(url);
  const r = await execa("curl", args, { timeout: opts.timeoutMs ?? 5_000, reject: false });
  return r.stdout.trim();
}

/**
 * Poll a URL until any HTTP status code comes back (even 401, 403, 404
 * count as "the server is up and responding"). Useful for waiting on
 * Icinga / Checkmk / similar that take a moment to bind and start
 * serving after the kernel accepts the port.
 */
export async function waitForHttp(
  url: string,
  opts: CurlOptions & { timeoutMs?: number; pollMs?: number } = {},
): Promise<void> {
  const deadline = Date.now() + (opts.timeoutMs ?? 60_000);
  let lastCode = "000";
  while (Date.now() < deadline) {
    lastCode = await curlHead(url, opts);
    if (lastCode !== "000") return;
    await new Promise((res) => setTimeout(res, opts.pollMs ?? 1000));
  }
  throw new Error(
    `No HTTP response from ${url} within ${opts.timeoutMs ?? 60_000}ms (last code=${lastCode})`,
  );
}
