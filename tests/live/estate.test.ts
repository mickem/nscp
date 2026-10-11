/**
 * Role-aware live suite for the Azure estate (build/powershell/run-estate.ps1).
 *
 * Reads an estate manifest and, for every machine that provisioned cleanly,
 * logs in to its agent and runs the checks its role declares in
 * build/powershell/estate/roles/<role>/role.json - the same checks, with the
 * same rules, as the estate gate (estate/verify-estate.ps1). The gate decides
 * the run's verdict; this suite is the jest-shaped record of it, and what a
 * developer runs by hand against a kept estate (-KeepVms) while iterating on
 * a check:
 *
 *   NSCP_ESTATE_MANIFEST=<...>/manifest.json npm run test:live -- estate
 *
 *   NSCP_ESTATE_MANIFEST  the manifest (provision-fleet-machines.ps1 writes
 *                         build/powershell/.fleet-machines.json; a run-estate
 *                         suite writes suite-<role>/manifest.json). Required;
 *                         without it the suite is skipped.
 *   NSCP_ESTATE_ROLES     the roles directory. Default
 *                         build/powershell/estate/roles.
 *
 * Each machine's address and web password come from the .vm.<name>.pwd file
 * the manifest points at, so the suite runs from the machine that provisioned
 * the estate.
 */
import * as fs from "fs";
import * as path from "path";
import { liveLogin, liveExecute, type LiveTarget } from "@fixtures/live-target";
import { messageOf, perfOf, type QueryResult } from "@fixtures/queries";

interface ManifestEntry {
  Name: string;
  Role?: string;
  Family?: string;
  Os?: string;
  Status?: string;
  PwdFile?: string;
  PublicIp?: string;
}

interface RoleCheck {
  command: string;
  args?: Record<string, string | string[]>;
  expect?: string;
  perf?: string[];
  os?: string[];
}

const STATES = ["OK", "WARNING", "CRITICAL", "UNKNOWN"];

const manifestPath = process.env.NSCP_ESTATE_MANIFEST;
const rolesDir =
  process.env.NSCP_ESTATE_ROLES ??
  path.resolve(__dirname, "..", "..", "build", "powershell", "estate", "roles");

/** PowerShell's ConvertTo-Json writes a one-element array as a bare object, and a BOM. */
function readJson<T>(file: string): T {
  return JSON.parse(fs.readFileSync(file, "utf8").replace(/^﻿/, "")) as T;
}

function machines(): ManifestEntry[] {
  if (!manifestPath) return [];
  const raw = readJson<ManifestEntry | ManifestEntry[]>(manifestPath);
  return (Array.isArray(raw) ? raw : [raw]).filter((m) => !m.Status || m.Status === "OK");
}

function roleChecks(role: string): RoleCheck[] {
  return readJson<{ checks?: RoleCheck[] }>(path.join(rolesDir, role, "role.json")).checks ?? [];
}

/** Mirrors Test-EstateCheckApplies: no "os" is everywhere, "linux" is every non-Windows family. */
function applies(check: RoleCheck, family: string): boolean {
  if (!check.os || check.os.length === 0) return true;
  if (check.os.includes(family)) return true;
  return family !== "windows" && check.os.includes("linux");
}

function label(check: RoleCheck): string {
  const args = Object.entries(check.args ?? {}).map(([k, v]) => `${k}=${[v].flat().join(",")}`);
  return [check.command, ...args].join(" ");
}

/** Mirrors Test-EstateCheckResult; returns why the result fails, or undefined. */
function judge(check: RoleCheck, q: QueryResult): string | undefined {
  const state = STATES[q.result];
  const expect = check.expect ?? "OK";
  if (!state) return `no result state (got ${q.result})`;
  if (expect === "any" && state === "UNKNOWN")
    return "returned UNKNOWN (expected any state but UNKNOWN)";
  if (expect !== "any" && state !== expect) return `returned ${state}, expected ${expect}`;
  const keys = Object.keys(perfOf(q));
  const missing = (check.perf ?? []).filter(
    (want) => !keys.some((k) => k.toLowerCase().includes(want.toLowerCase())),
  );
  if (missing.length > 0)
    return `no perfdata matching ${missing.join(", ")} (got: ${keys.join(", ") || "none"})`;
  return undefined;
}

/** Mirrors verify-estate.ps1: the pwd file's own "Web URL" first, else the public IP on 8443. */
function credentials(m: ManifestEntry): { url?: string; password?: string } {
  const fallback = m.PublicIp ? `https://${m.PublicIp}:8443` : undefined;
  if (!m.PwdFile || !fs.existsSync(m.PwdFile)) return { url: fallback };
  const text = fs.readFileSync(m.PwdFile, "utf8");
  const field = (name: string) => new RegExp(`^\\s*${name}:\\s*(.+?)\\s*$`, "m").exec(text)?.[1];
  const ip = field("Public IP");
  const url = field("Web URL") ?? fallback ?? (ip ? `https://${ip}:8443` : undefined);
  return { url, password: field("Web Password") };
}

const describeEstate = manifestPath ? describe : describe.skip;

describeEstate("estate role checks", () => {
  const list = machines();

  it("the manifest lists at least one provisioned machine", () => {
    expect(list.length).toBeGreaterThan(0);
  });

  for (const m of list) {
    const role = m.Role || "base";
    const family = m.Family || m.Os || "linux";
    const checks = roleChecks(role).filter((c) => applies(c, family));

    describe(`${m.Name} (${role}, ${family})`, () => {
      let target: LiveTarget;

      beforeAll(async () => {
        const { url, password } = credentials(m);
        if (!url || !password)
          throw new Error(`${m.Name}: no web address or password in ${m.PwdFile}`);
        target = await liveLogin({
          base: url,
          user: "admin",
          password,
          insecure: true,
        });
      });

      for (const check of checks) {
        it(label(check), async () => {
          // A collector warming up or a bundle reload in flight: give it a
          // minute, as the gate does over several rounds.
          const deadline = Date.now() + 60_000;
          let q: QueryResult;
          let why: string | undefined;
          for (;;) {
            q = await liveExecute(target, check.command, check.args as Record<string, string>);
            why = judge(check, q);
            if (!why || Date.now() >= deadline) break;
            await new Promise((r) => setTimeout(r, 5_000));
          }
          // The reason and the check's own output, so a failure reads on its own.
          expect(why ? `${why} :: ${messageOf(q)}` : undefined).toBeUndefined();
        });
      }
    });
  }
});
