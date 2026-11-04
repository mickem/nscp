/**
 * Scenario 5: a reload soak. The REST flood (scenario 1) and the scheduler
 * load (scenario 3) run together while the test reloads a module every few
 * seconds and writes settings over REST in between - the fleet-sync-stress
 * idea, aimed at the loadModuleEx re-entry rule from CLAUDE.md: a settings
 * reload calls loadModuleEx again on the live module while its worker threads
 * are running and its commands are being dispatched.
 *
 * Every round: PUT /api/v2/settings writes the round number, a Lua helper
 * turns it into a CheckHelpers alias (`soak_probe = check_ok message=round-<n>`,
 * no space: an alias is split on spaces) in the same in-memory store, and one module of the rotation is
 * reloaded through Core:reload. The round is complete when the core logs the
 * reload and, for a CheckHelpers round, when `soak_probe` answers `round-<n>`
 * over REST - so each reload is seen to apply the settings written before
 * it, and an alias that was re-registered or dropped by the reload shows up
 * as a failed round. The rotation is CheckHelpers (the module the flood is
 * dispatched into; its alias list is rebuilt per load), the Scheduler (its
 * pool is replaced under the running schedules) and LUAScript (the scripts
 * are reloaded, the helper that asked for the reload among them). WEBServer
 * is left out on purpose: its listener and sessions are read at start, not on
 * a reload, so reloading it proves nothing the others do not.
 *
 * Throughout, N REST workers run `check_ok`, the `soak_probe` alias and the
 * metrics tree, and `SCHEDULES` schedules at 5 s report into the result
 * cache. Pass/fail is on the invariants in stress/lib/invariants.ts plus:
 * every round completed, no `refused to reload` and no `Exception raised when
 * reloading` in the log, every schedule seen, every beat OK. Reloads that
 * raced a running call (`Reloaded ... while calls into it were still
 * running`) are counted and recorded, not asserted.
 *
 * The default window is three minutes, long enough for every module in the
 * rotation to reload a dozen times; NSCP_STRESS_DURATION=1800 makes it a
 * nightly soak. NSCP_STRESS_RELOAD_EVERY (seconds, default 5) is the pause
 * between rounds.
 */
import * as fs from "fs";
import * as path from "path";

import { NscpInstance, OK, setupQueryNscp } from "@fixtures/index";

import { expectCleanLog, expectNoGrowth, expectResponsive } from "@stress/invariants";
import { readKnobs, scenarioTimeoutMs, warmupMs } from "@stress/knobs";
import { percentiles, runLoad } from "@stress/load";
import { startSampler } from "@stress/process-stats";
import { writeReport } from "@stress/report";
import { RestClient } from "@stress/rest-client";

const base = readKnobs();
/** Three minutes unless the knob says otherwise; a soak is 1800. */
const knobs = {
  ...base,
  durationSeconds: process.env.NSCP_STRESS_DURATION ? base.durationSeconds : 180,
};
jest.setTimeout(scenarioTimeoutMs(knobs));

const RELOAD_EVERY_MS = Number(process.env.NSCP_STRESS_RELOAD_EVERY ?? 5) * 1000;
const SCHEDULES = Number(process.env.NSCP_STRESS_SCHEDULES ?? 200);
const INTERVAL_S = 5;
const SCHED_ALIAS = "soak_sched";
const SCHED_BASE = `/settings/${SCHED_ALIAS}`;
const CHANNEL = "SOAK_RESULTS";
const POLL_MS = 1_000;
const ROTATION = ["CheckHelpers", SCHED_ALIAS, "LUAScript"];

const LUA = `
-- Reload the named modules (REST passes a parameter with no value as a bare
-- argument). From inside a REST call the core hands the reload to the
-- scheduler, so this returns before it has applied.
local function reload_modules(command, args)
  local core = Core()
  for _, m in ipairs(args) do core:reload(m) end
  return 'ok', 'reload requested: ' .. table.concat(args, ',')
end
Registry():simple_function('reload_modules', reload_modules, 'reload the named modules')

-- Turn the round the test wrote under /settings/soak into the alias the
-- next CheckHelpers reload registers. The alias section has a space in its
-- name, which the REST settings route does not decode, so the write goes
-- through the same in-memory store from here.
local function soak_alias(command, args)
  local round = Settings():get_string('/settings/soak', 'round', 'unset')
  Settings():set_string('/settings/check helpers/alias', 'soak_probe', 'check_ok message=round-' .. round)
  return 'ok', 'alias set for round ' .. round
end
Registry():simple_function('soak_alias', soak_alias, 'write the soak_probe alias for the current round')

-- The legacy teardown: an empty pool applied by reloading the scheduler.
local function sched_stop(command, args)
  Settings():set_string('${SCHED_BASE}', 'threads', '0')
  Core():reload('${SCHED_ALIAS}')
  return 'ok', 'stopping'
end
Registry():simple_function('sched_stop', sched_stop, 'stop the soak scheduler')
`;

interface CachedResult {
  alias: string;
  status: number;
  count: number;
}

interface Round {
  round: number;
  module: string;
  /** From the settings write to the core logging the reload. */
  reloadMs: number;
  /** For a CheckHelpers round: until soak_probe answered with the round. */
  applyMs?: number;
}

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

describe("stress: reload soak under a REST flood and a scheduler load", () => {
  let nscp: NscpInstance;
  let client: RestClient;

  /** How many times the core has started reloading `module` so far. */
  const reloadsOf = (module: string) =>
    nscp
      .capturedStdout()
      .split(/\r?\n/)
      .filter((l) => l.includes(`Reloading: ${module}`)).length;

  beforeAll(async () => {
    nscp = new NscpInstance();
    const script = path.join(nscp.scratch("lua"), "soak.lua");
    fs.writeFileSync(script, LUA);
    await nscp.configure({
      "/modules": {
        CheckHelpers: "enabled",
        LUAScript: "enabled",
        WEBServer: "enabled",
        [SCHED_ALIAS]: "Scheduler",
      },
      "/settings/lua/scripts": { soak: script },
      "/settings/soak": { round: "0" },
      "/settings/check helpers/alias": { soak_probe: "check_ok message=round-0" },
      "/settings/WEB/server/results": {
        enabled: "true",
        channel: CHANNEL,
        "primary index": "${alias-or-command}",
      },
      [`${SCHED_BASE}/schedules/default`]: {
        channel: CHANNEL,
        command: "check_ok",
        interval: `${INTERVAL_S}s`,
        randomness: "0%",
        report: "all",
      },
    });
    // The schedules go straight into the INI, as scheduler-load does.
    const lines = [`[${SCHED_BASE}/schedules]`];
    for (let i = 1; i <= SCHEDULES; i++)
      lines.push(`soak_${String(i).padStart(5, "0")} = check_ok`);
    fs.appendFileSync(nscp.settingsFile, `\n${lines.join("\n")}\n`);

    await setupQueryNscp(nscp, "CheckHelpers");
    client = new RestClient(knobs.concurrency);
    await client.login();
    // /api/v2/metrics is an empty body until the core's first snapshot (every
    // `metrics interval`, 10 s by default); see rest-flood.
    const metricsDeadline = performance.now() + 30_000;
    for (;;) {
      if ((await client.get("/api/v2/metrics")) !== "") break;
      if (performance.now() >= metricsDeadline)
        throw new Error("no metrics snapshot within 30 s of boot");
      await sleep(500);
    }
  });

  afterAll(async () => {
    await client?.close();
    await nscp?.stop();
  });

  /** One round: write the round, ask for the reload, wait until it has applied. */
  async function reloadRound(n: number): Promise<Round> {
    const module = ROTATION[n % ROTATION.length];
    const t0 = performance.now();
    await client.putJson("/api/v2/settings/settings/soak", [{ key: "round", value: String(n) }]);
    await client.query("soak_alias");
    const before = reloadsOf(module);
    await client.query("reload_modules", { [module]: "" });
    const logDeadline = performance.now() + 30_000;
    while (reloadsOf(module) <= before) {
      if (performance.now() >= logDeadline)
        throw new Error(`the core did not log a reload of ${module} within 30 s`);
      await sleep(100);
    }
    const reloadMs = performance.now() - t0;
    if (module !== "CheckHelpers") return { round: n, module, reloadMs };
    const applyDeadline = performance.now() + 15_000;
    for (;;) {
      const q = await client.query("soak_probe");
      const message = q.lines?.[0]?.message ?? "";
      if (message === `round-${n}`) break;
      if (performance.now() >= applyDeadline) {
        throw new Error(
          `soak_probe still answers ${JSON.stringify(message)} 15 s after the reload of round ${n}`,
        );
      }
      await sleep(100);
    }
    return { round: n, module, reloadMs, applyMs: performance.now() - t0 };
  }

  it("keeps answering and keeps every schedule while modules reload under it", async () => {
    const pid = nscp.pid;
    if (pid === undefined) throw new Error("the agent is not running");
    const sampler = startSampler(pid);

    // The REST mix: six in ten check_ok, two in ten the alias the rounds
    // rewrite (any round's answer is right, as long as it is one), two in
    // ten the metrics tree.
    const mix = { check_ok: 0, soak_probe: 0, metrics: 0 };
    const task = async (_worker: number, n: number): Promise<void> => {
      const slot = n % 10;
      if (slot < 6) {
        mix.check_ok++;
        await client.query("check_ok");
      } else if (slot < 8) {
        mix.soak_probe++;
        const q = await client.query("soak_probe");
        const message = q.lines?.[0]?.message ?? "";
        if (!/^round-\d+$/.test(message))
          throw new Error(`soak_probe answered ${JSON.stringify(message)}`);
      } else {
        mix.metrics++;
        const metrics = await client.getJson<Record<string, unknown>>("/api/v2/metrics");
        if (Object.keys(metrics).length === 0)
          throw new Error("/api/v2/metrics answered an empty tree");
      }
    };

    // The scheduler's beats, drained once a second for the whole run.
    const perAlias = new Map<string, number>();
    let delivered = 0;
    let notOk = 0;
    const drain = async (): Promise<void> => {
      const results = await client.getJson<CachedResult[]>("/api/v2/results");
      for (const r of results) {
        delivered += r.count;
        perAlias.set(r.alias, (perAlias.get(r.alias) ?? 0) + r.count);
        if (r.status !== OK) notOk += r.count;
      }
    };
    let draining = true;
    const drainLoop = (async () => {
      while (draining) {
        await sleep(POLL_MS);
        await drain();
      }
    })();

    // The rounds, from the start of the warm-up (so the baseline RSS is one
    // the reloads have already shaped) to the end of the run.
    const rounds: Round[] = [];
    const roundFailures: string[] = [];
    const roundsUntil = performance.now() + warmupMs(knobs) + knobs.durationSeconds * 1000;
    const reloadLoop = (async () => {
      for (let n = 1; performance.now() < roundsUntil; n++) {
        try {
          rounds.push(await reloadRound(n));
        } catch (e) {
          roundFailures.push(
            `round ${n} (${ROTATION[n % ROTATION.length]}): ${(e as Error).message}`,
          );
        }
        await sleep(RELOAD_EVERY_MS);
      }
    })();

    const warmup = await runLoad({
      durationMs: warmupMs(knobs),
      concurrency: knobs.concurrency,
      rps: knobs.rps,
      task,
    });
    await sampler.markBaseline();
    const counted = { ...mix };
    perAlias.clear();
    const deliveredBefore = delivered;

    const started = performance.now();
    const load = await runLoad({
      durationMs: knobs.durationSeconds * 1000,
      concurrency: knobs.concurrency,
      rps: knobs.rps,
      task,
    });
    const elapsedSeconds = (performance.now() - started) / 1000;
    await reloadLoop;
    await sampler.markEnd();

    // Stop the pool, then the agent must answer a fresh check promptly. The
    // reload is deferred inside the agent, so give it a moment to land.
    await client.query("sched_stop");
    await sleep(2_000);
    const responsiveAfterMs = await expectResponsive(() => client.query("check_ok"));
    draining = false;
    await drainLoop;
    await drain();
    const samples = await sampler.stop();

    const log = nscp.capturedStdout().split(/\r?\n/);
    const refused = log.filter((l) => /refused to reload|Exception raised when reloading/.test(l));
    const raced = log.filter((l) => /while calls into it were still running/.test(l)).length;
    const perModule = Object.fromEntries(
      ROTATION.map((m) => [m, rounds.filter((r) => r.module === m).length]),
    );

    const growth = expectNoGrowth(sampler, knobs.rssTolerancePct);
    writeReport({
      scenario: "reload-soak",
      startedAt: new Date(Date.now() - samples[samples.length - 1].t * 1000).toISOString(),
      platform: process.platform,
      knobs,
      load,
      responsiveAfterMs,
      growth,
      samples,
      extra: {
        warmup,
        mix: {
          check_ok: mix.check_ok - counted.check_ok,
          soak_probe: mix.soak_probe - counted.soak_probe,
          metrics: mix.metrics - counted.metrics,
        },
        reloadEverySeconds: RELOAD_EVERY_MS / 1000,
        rounds: rounds.length,
        roundsPerModule: perModule,
        roundFailures,
        reloadMs: percentiles(rounds.map((r) => r.reloadMs).sort((a, b) => a - b)),
        applyMs: percentiles(
          rounds
            .map((r) => r.applyMs)
            .filter((ms): ms is number => ms !== undefined)
            .sort((a, b) => a - b),
        ),
        reloadRaces: raced,
        schedules: SCHEDULES,
        intervalSeconds: INTERVAL_S,
        elapsedSeconds,
        delivered: delivered - deliveredBefore,
        expectedBeats: Math.floor(elapsedSeconds / INTERVAL_S) * SCHEDULES,
        notOk,
        schedulesSeen: perAlias.size,
      },
    });

    // Zero failed requests, in the warm-up as well as the run.
    expect(warmup.errors).toEqual({ count: 0, samples: [] });
    expect(load.errors).toEqual({ count: 0, samples: [] });
    expect(load.requests).toBeGreaterThan(0);
    // Every round completed and every module of the rotation was reloaded.
    expect({ failures: roundFailures, count: roundFailures.length }).toEqual({
      failures: [],
      count: 0,
    });
    for (const m of ROTATION)
      expect({ module: m, rounds: perModule[m] }).not.toEqual({ module: m, rounds: 0 });
    expect({ refused, count: refused.length }).toEqual({ refused: [], count: 0 });
    // Every result was check_ok's OK; anything else is a failed dispatch.
    expect(notOk).toBe(0);
    expect(delivered).toBeGreaterThan(0);
    // No schedule starved by a pool replaced under it: with at least two
    // intervals in the window each one reported at least once.
    if (elapsedSeconds >= 2 * INTERVAL_S) {
      const missing = Array.from(
        { length: SCHEDULES },
        (_, i) => `soak_${String(i + 1).padStart(5, "0")}`,
      ).filter((a) => !perAlias.has(a));
      expect({ missing: missing.slice(0, 20), count: missing.length }).toEqual({
        missing: [],
        count: 0,
      });
    }
    expectCleanLog(nscp);
  });
});
