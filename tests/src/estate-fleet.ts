/**
 * The fleet server's side of the estate's fault scenarios
 * (live/estate.test.ts, a role's "faults"): the failure reports an agent
 * sent it, and the explanation of an error it gives back.
 *
 * The server and its API key come from the same places the build/powershell
 * scripts read them: NSCLIENT_FLEET_SERVER (else the machine's manifest
 * entry) and NSCLIENT_FLEET_API_KEY, else .fleet-config.json next to the
 * scripts (NSCLIENT_FLEET_CONFIG points elsewhere). NSCP_ESTATE_FLEET_INSECURE=1
 * skips verifying the server's certificate, like the scripts' -FleetNoVerify.
 */
import * as fs from "fs";
import * as path from "path";
import request from "supertest";
import { POWERSHELL_DIR, readJson } from "./estate-manifest";
import type { RoleFault } from "./estate-roles";

export interface FleetApi {
  base: string;
  key: string;
  insecure: boolean;
}

/** One stored failure report, as GET /api/hosts/{id}/contexts lists it. */
export interface FleetContext {
  id: string;
  command: string;
  alias: string;
  status_word: string;
  message: string;
  context_state: string;
  metrics_count: number;
  reported_at: number;
}

/** GET /api/settings/explain, the fields the fault scenarios depend on. */
export interface ExplainSettings {
  reports_enabled: boolean;
  min_interval_secs: number;
  explain_enabled: boolean;
  provider: string;
  model: string;
  daily_call_budget: number;
  calls_today: number;
}

export interface Explanation {
  summary: string;
  relevant_evidence: string[];
  likely_causes: string[];
  suggested_checks: string[];
  confidence: string;
}

/** POST /api/explain's answer. */
export interface ExplainResult {
  host_id: string;
  hostname: string;
  explanation: Explanation;
  check: FleetContext | null;
  snapshot: FleetContext | null;
  facts_sources: string[];
  provider: string;
  model: string;
  input_tokens: number;
  output_tokens: number;
}

interface FleetConfigFile {
  server?: string;
  apiKey?: string;
}

function configFile(): FleetConfigFile {
  const file = process.env.NSCLIENT_FLEET_CONFIG || path.join(POWERSHELL_DIR, ".fleet-config.json");
  if (!fs.existsSync(file)) return {};
  try {
    return readJson<FleetConfigFile>(file);
  } catch (e) {
    throw new Error(`could not read the fleet config ${file}: ${String(e)}`);
  }
}

/** The fleet server to ask about a machine; throws saying what is missing. */
export function fleetApi(manifestServer?: string): FleetApi {
  const file = configFile();
  const base = (process.env.NSCLIENT_FLEET_SERVER || manifestServer || file.server || "").replace(
    /\/+$/,
    "",
  );
  const key = process.env.NSCLIENT_FLEET_API_KEY || file.apiKey || "";
  if (!base)
    throw new Error(
      "no fleet server: set NSCLIENT_FLEET_SERVER or put it in build/powershell/.fleet-config.json",
    );
  if (!key)
    throw new Error(
      "no fleet API key: set NSCLIENT_FLEET_API_KEY or put it in build/powershell/.fleet-config.json",
    );
  const raw = process.env.NSCP_ESTATE_FLEET_INSECURE ?? "";
  const insecure = raw !== "" && raw !== "0" && raw.toLowerCase() !== "false";
  return { base, key, insecure };
}

async function call(
  api: FleetApi,
  method: "GET" | "POST",
  apiPath: string,
  body?: unknown,
): Promise<{ status: number; body: any; text: string }> {
  const agent = request(api.base);
  let req = method === "POST" ? agent.post(apiPath).send(body as object) : agent.get(apiPath);
  req = req.set("Authorization", `Bearer ${api.key}`).set("Accept", "application/json");
  if (api.insecure) req = req.disableTLSCerts();
  const res = await req.timeout({ response: 180_000, deadline: 240_000 }).ok(() => true);
  return { status: res.status, body: res.body, text: res.text ?? "" };
}

function failed(what: string, res: { status: number; text: string }): Error {
  return new Error(`${what} answered HTTP ${res.status}: ${res.text.slice(0, 300)}`);
}

/**
 * The tenant's explain settings, and why the fault scenarios cannot run
 * against them (undefined when they can). Reports have to be on; so do
 * explanations, with a provider; and the tenant's floor on the report
 * interval has to leave each scenario its own report within a few minutes.
 */
export async function explainReadiness(
  api: FleetApi,
  faults: number,
  maxIntervalSeconds: number,
): Promise<{ settings?: ExplainSettings; problem?: string }> {
  const res = await call(api, "GET", "/api/settings/explain");
  if (res.status === 404)
    return {
      problem: `${api.base} has no /api/settings/explain: the fleet server predates failure context and explanations`,
    };
  if (res.status !== 200) return { problem: failed("GET /api/settings/explain", res).message };
  const settings = res.body as ExplainSettings;
  if (!settings.reports_enabled)
    return {
      settings,
      problem: "the tenant has failure reports switched off (Settings, reports_enabled)",
    };
  if (!settings.explain_enabled)
    return {
      settings,
      problem:
        "the tenant has explanations switched off: configure a provider under Settings in the fleet console " +
        "(or PUT /api/settings/explain) before running the faults role",
    };
  if (settings.min_interval_secs > maxIntervalSeconds)
    return {
      settings,
      problem:
        `the tenant asks agents to report at most every ${settings.min_interval_secs}s ` +
        `(min_interval_secs); the faults role needs a report within ${maxIntervalSeconds}s of each fault`,
    };
  const left = settings.daily_call_budget - settings.calls_today;
  if (settings.daily_call_budget > 0 && left < faults)
    return {
      settings,
      problem: `only ${left} explanation(s) left in today's budget of ${settings.daily_call_budget}, and ${faults} fault(s) to explain`,
    };
  return { settings };
}

export async function hostContexts(api: FleetApi, hostId: string): Promise<FleetContext[]> {
  const res = await call(api, "GET", `/api/hosts/${encodeURIComponent(hostId)}/contexts`);
  if (res.status !== 200) throw failed(`GET /api/hosts/${hostId}/contexts`, res);
  return res.body as FleetContext[];
}

/** The newest stored report of a command, if any. */
export function latestReport(contexts: FleetContext[], command: string): FleetContext | undefined {
  return contexts
    .filter((c) => c.command === command)
    .sort((a, b) => b.reported_at - a.reported_at)[0];
}

export async function explain(
  api: FleetApi,
  hostId: string,
  error: string,
  check: string,
): Promise<ExplainResult> {
  const res = await call(api, "POST", "/api/explain", { host: hostId, error, check });
  if (res.status !== 200) throw failed("POST /api/explain", res);
  return res.body as ExplainResult;
}

/** Everything the model wrote, lowercased, to look for a word in. */
function explanationText(e: Explanation): string {
  return [e.summary, ...e.relevant_evidence, ...e.likely_causes, ...e.suggested_checks]
    .join("\n")
    .toLowerCase();
}

/** Which of `words` the explanation names, case-insensitively. */
export function named(e: Explanation, words: string[] = []): string[] {
  const text = explanationText(e);
  return words.filter((w) => text.includes(w.toLowerCase()));
}

/**
 * Why an explanation of a fault does not hold up, or undefined. It must be
 * built on the evidence the fault produced - the failed check's own report,
 * a metrics snapshot and the agent's facts - and name what the fault is
 * about. The model's wording is otherwise free; whether it found the culprit
 * is recorded by the caller, not judged here.
 */
export function judgeExplanation(fault: RoleFault, r: ExplainResult): string | undefined {
  if (!r.explanation || !r.explanation.summary?.trim()) return "the explanation has no summary";
  if (!r.check) return "the explanation was not based on any report of the check";
  if (r.check.command !== fault.check.command)
    return `the explanation used the report of ${r.check.command}, not ${fault.check.command}`;
  if (!r.snapshot) return "the explanation had no metrics snapshot to go on";
  if (!(r.facts_sources ?? []).includes("agent"))
    return `the explanation had no facts from the agent (sources: ${(r.facts_sources ?? []).join(", ") || "none"})`;
  const mentions = fault.explain?.mentions ?? [];
  if (mentions.length > 0 && named(r.explanation, mentions).length === 0)
    return `the explanation names none of ${mentions.join(", ")}: "${r.explanation.summary}"`;
  return undefined;
}
