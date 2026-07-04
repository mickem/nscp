/**
 * "Live target" client for the remote acceptance suite (tests/live/**).
 *
 * Unlike the rest of the harness — which spawns its own throwaway nscp via
 * NscpInstance and talks to https://127.0.0.1:8443 — these helpers connect to
 * an nscp that is *already installed and running* somewhere else (a freshly
 * provisioned Azure VM, a locally-installed service, your dev build started by
 * hand). Nothing is spawned or configured here; the server is expected to be
 * up with WEBServer enabled and a known password.
 *
 * Configuration is entirely by environment so the exact same suite runs from a
 * pipeline against a VM's public IP and locally against localhost:
 *
 *   NSCP_TARGET_URL       Base URL of the REST API. Default https://127.0.0.1:8443
 *   NSCP_TARGET_USER      REST user to log in as.    Default "admin"
 *   NSCP_TARGET_PASSWORD  Password for that user.    REQUIRED
 *   NSCP_TARGET_INSECURE  Skip TLS cert verification (VMs use a self-signed
 *                         cert). Default "1" (on); set to "0"/"false" to verify.
 *   NSCP_TARGET_OS        "windows" | "linux" — lets platform-specific checks
 *                         run without querying the server. Optional; when unset
 *                         the suite derives it from check_os_version.
 */
import request from "supertest";
import type { QueryResult } from "./queries";

export type TargetOs = "windows" | "linux";

export interface LiveTarget {
  base: string;
  user: string;
  key: string;
  insecure: boolean;
}

export interface LiveConfig {
  base: string;
  user: string;
  password: string;
  insecure: boolean;
  os?: TargetOs;
}

/** superagent request tweaked for the target's TLS posture. */
function withTls<T extends request.Test>(req: T, insecure: boolean): T {
  // disableTLSCerts() accepts any cert (self-signed VM cert); trustLocalhost()
  // is the stricter localhost-only path used when the caller opts back in.
  return insecure ? (req.disableTLSCerts() as T) : (req.trustLocalhost(true) as T);
}

/** Read + validate the NSCP_TARGET_* environment. Throws if password missing. */
export function liveConfig(): LiveConfig {
  const base = process.env.NSCP_TARGET_URL ?? "https://127.0.0.1:8443";
  const user = process.env.NSCP_TARGET_USER ?? "admin";
  const password = process.env.NSCP_TARGET_PASSWORD ?? "";
  const insecureRaw = process.env.NSCP_TARGET_INSECURE ?? "1";
  const insecure = insecureRaw !== "0" && insecureRaw.toLowerCase() !== "false";
  const osRaw = process.env.NSCP_TARGET_OS?.toLowerCase();
  const os: TargetOs | undefined =
    osRaw === "windows" ? "windows" : osRaw === "linux" ? "linux" : undefined;
  if (!password) {
    throw new Error(
      "NSCP_TARGET_PASSWORD is not set. The live suite needs the WEB password of " +
        `the running server (${base}). Set NSCP_TARGET_URL / NSCP_TARGET_PASSWORD ` +
        "and re-run (see tests/live/README.md).",
    );
  }
  return { base, user, password, insecure, os };
}

/** Log in over REST and return a target handle carrying the Bearer key. */
export async function liveLogin(cfg: LiveConfig = liveConfig()): Promise<LiveTarget> {
  const res = await withTls(
    request(cfg.base).get("/api/v1/login").auth(cfg.user, cfg.password),
    cfg.insecure,
  ).expect(200);
  const key = res.body?.key as string | undefined;
  if (!key) {
    throw new Error(`Login to ${cfg.base} as ${cfg.user} returned no key`);
  }
  return { base: cfg.base, user: cfg.user, key, insecure: cfg.insecure };
}

/** Raw version string from /api/v1/info. */
export async function liveVersion(t: LiveTarget): Promise<string> {
  const res = await withTls(
    request(t.base).get("/api/v1/info").set("Authorization", `Bearer ${t.key}`),
    t.insecure,
  ).expect(200);
  return (res.body?.version as string) ?? "";
}

/** Execute one query via /api/v1/queries/<cmd>/commands/execute. */
export async function liveExecute(
  t: LiveTarget,
  command: string,
  args: Record<string, string> = {},
): Promise<QueryResult> {
  const res = await withTls(
    request(t.base)
      .get(`/api/v1/queries/${command}/commands/execute`)
      .query(args)
      .set("Authorization", `Bearer ${t.key}`),
    t.insecure,
  ).expect(200);
  return res.body as QueryResult;
}

/**
 * Re-run a query until `until(result)` holds (collector warm-up on the remote
 * host, history accumulation, …). Returns the last result either way so a
 * timeout still fails on the real payload.
 */
export async function livePoll(
  t: LiveTarget,
  command: string,
  args: Record<string, string>,
  until: (q: QueryResult) => boolean,
  timeoutMs = 30_000,
): Promise<QueryResult> {
  const deadline = Date.now() + timeoutMs;
  let last: QueryResult | undefined;
  for (;;) {
    last = await liveExecute(t, command, args);
    if (until(last) || Date.now() >= deadline) return last;
    await new Promise((r) => setTimeout(r, 500));
  }
}

/**
 * Best-effort OS of the target: NSCP_TARGET_OS if set, else derived from
 * check_os_version's message ("Windows …" vs a Linux distro string). Returns
 * undefined only if the check is unavailable and no override was given.
 */
export async function liveDetectOs(t: LiveTarget): Promise<TargetOs | undefined> {
  const fromEnv = liveConfig().os;
  if (fromEnv) return fromEnv;
  try {
    const q = await liveExecute(t, "check_os_version");
    const msg = (q.lines ?? []).map((l) => l.message).join(" ");
    return /windows/i.test(msg) ? "windows" : "linux";
  } catch {
    return undefined;
  }
}
