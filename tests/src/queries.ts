/**
 * Fixture for command/query integration suites (checksystem-commands,
 * checkdisk-commands, …): boots nscp with one check module + WEBServer,
 * logs in, and executes queries over the REST API.
 *
 * REST (rather than one-shot `nscp client --boot`) is load-bearing here:
 * several checks read from a 1 Hz background collector (CPU / memory /
 * network buffers, disk I/O counters, process history), so the process
 * must stay up long enough for the collector to produce samples. Use
 * `pollQuery` to wait out that warm-up deterministically.
 */
import request from "supertest";

import type { NscpInstance, SettingsTree } from "./nscp";
import { REST_URL } from "./rest-fixture";

export interface QueryPerfEntry {
  value: number | string;
  unit?: string;
  warning?: number | string;
  critical?: number | string;
  minimum?: number | string;
  maximum?: number | string;
}

export interface QueryLine {
  message: string;
  perf?: Record<string, QueryPerfEntry>;
}

export interface QueryResult {
  command: string;
  /** Nagios status: 0 OK, 1 WARNING, 2 CRITICAL, 3 UNKNOWN. */
  result: number;
  lines: QueryLine[];
}

export const OK = 0;
export const WARNING = 1;
export const CRITICAL = 2;
export const UNKNOWN = 3;

/** All messages of a result joined into one string, for regex asserts. */
export function messageOf(q: QueryResult): string {
  return (q.lines ?? []).map((l) => l.message).join("\n");
}

/** Merged perf entries across all lines of a result. */
export function perfOf(q: QueryResult): Record<string, QueryPerfEntry> {
  const out: Record<string, QueryPerfEntry> = {};
  for (const line of q.lines ?? []) {
    Object.assign(out, line.perf ?? {});
  }
  return out;
}

/** Numeric value of one perf entry, or undefined when absent/non-numeric. */
export function perfValue(q: QueryResult, label: string): number | undefined {
  const v = perfOf(q)[label]?.value;
  return typeof v === "number" ? v : undefined;
}

/**
 * Configure and start an nscp instance with `module` + WEBServer enabled,
 * then log in as admin. Returns the Bearer key for executeQuery().
 */
export async function setupQueryNscp(
  nscp: NscpInstance,
  module: string,
  extraSettings: SettingsTree = {},
): Promise<string> {
  await nscp.configure({
    "/modules": {
      [module]: "enabled",
      WEBServer: "enabled",
    },
    "/settings/default": {
      password: "default-password",
      "allowed hosts": "127.0.0.1,::1",
    },
    "/settings/WEB/server/roles": {
      full: "*",
    },
    "/settings/WEB/server/users/admin": {
      role: "full",
      password: "default-password",
    },
    ...extraSettings,
  });

  nscp.start();
  await nscp.waitForPort(8443, { timeoutMs: 30_000 });

  const login = await request(REST_URL)
    .get("/api/v1/login")
    .auth("admin", "default-password")
    .trustLocalhost(true)
    .expect(200);
  return login.body.key as string;
}

/** Execute one query via /api/v1/queries/<cmd>/commands/execute.
 *
 * An array value repeats the parameter (e.g. `{ file: ["Application", "System"] }`
 * becomes `file=Application&file=System`), which some checks use to pass an
 * option multiple times. */
export async function executeQuery(
  key: string,
  command: string,
  args: Record<string, string | string[]> = {},
): Promise<QueryResult> {
  const res = await request(REST_URL)
    .get(`/api/v1/queries/${command}/commands/execute`)
    .query(args)
    .set("Authorization", `Bearer ${key}`)
    .trustLocalhost(true)
    .expect(200);
  return res.body as QueryResult;
}

/**
 * Re-run a query until `until(result)` holds (collector warm-up, history
 * accumulation, …). Returns the last result either way — callers assert
 * on it so a timeout fails with the actual final payload in the diff.
 */
export async function pollQuery(
  key: string,
  command: string,
  args: Record<string, string>,
  until: (q: QueryResult) => boolean,
  timeoutMs = 30_000,
): Promise<QueryResult> {
  const deadline = Date.now() + timeoutMs;
  let last: QueryResult | undefined;
  for (;;) {
    last = await executeQuery(key, command, args);
    if (until(last) || Date.now() >= deadline) return last;
    await new Promise((r) => setTimeout(r, 500));
  }
}
