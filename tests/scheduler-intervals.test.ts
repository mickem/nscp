/**
 * The Scheduler fires each schedule at its configured interval, routes the
 * results to the schedule's channel, and stops when its pool is set to zero
 * threads.
 *
 * Port of scripts/python/test_scheduler.py with the looser contract of its Lua
 * port (scripts/lua/test_scheduler.lua): the Python original sampled for 90 s
 * and pinned every count to a narrow band, which was flaky on busy CI agents.
 * Here the window is 20 s, each interval gets a plausible band, and the
 * assertions that carry the weight are ordering invariants (a faster interval
 * fired more often) rather than exact counts.
 *
 * A Lua `simple_function` is the scheduled check. It counts its invocations
 * keyed by its argument, which each schedule sets to its own name, and a Lua
 * subscription counts the results the scheduler submits on the channel. The
 * counters are read over REST through a `sched_counts` query served by the
 * same script.
 */
import * as path from "path";
import * as fs from "fs";

import { NscpInstance, OK, executeQuery, messageOf, setupQueryNscp } from "@fixtures/index";

jest.setTimeout(180_000);

/** Scheduler instance alias; its settings live under /settings/<alias>. */
const ALIAS = "test_sched";
const BASE = `/settings/${ALIAS}`;
const CHANNEL = "test_sched_results";
const COMMAND = "test_sched_cmd";
const WINDOW_MS = 20_000;

const LUA = `
local counts = {}
local results = 0

local function check(command, args)
  local key = args[1] or 'none'
  counts[key] = (counts[key] or 0) + 1
  return 'ok', key
end

local function on_result(channel, command, result, lines)
  results = results + 1
  -- A subscription answers (success, message); a status string reads as a
  -- failed submit.
  return true, 'received'
end

local function sched_counts(command, args)
  local parts = { 'results=' .. results }
  for k, v in pairs(counts) do table.insert(parts, k .. '=' .. v) end
  return 'ok', table.concat(parts, ',')
end

local function sched_reset(command, args)
  counts = {}
  results = 0
  return 'ok', 'reset'
end

-- The legacy teardown: an empty pool, applied by reloading the instance.
local function sched_stop(command, args)
  Settings():set_string('${BASE}', 'threads', '0')
  Core():reload('${ALIAS}')
  return 'ok', 'stopping'
end

local reg = Registry()
reg:simple_function('${COMMAND}', check, 'counts its own invocations')
reg:simple_function('sched_counts', sched_counts, 'invocation counts so far')
reg:simple_function('sched_reset', sched_reset, 'zero the counters')
reg:simple_function('sched_stop', sched_stop, 'stop the scheduler')
reg:simple_subscription('${CHANNEL}', on_result, 'scheduler result sink')
`;

describe("Scheduler intervals", () => {
  let nscp: NscpInstance;
  let key: string;

  async function counts(): Promise<Record<string, number>> {
    const q = await executeQuery(key, "sched_counts");
    expect(q.result).toBe(OK);
    const out: Record<string, number> = {};
    for (const pair of messageOf(q).split(",")) {
      const [k, v] = pair.split("=");
      out[k] = Number(v);
    }
    return out;
  }

  const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

  beforeAll(async () => {
    nscp = new NscpInstance();
    const script = path.join(nscp.scratch("lua"), "sched.lua");
    fs.writeFileSync(script, LUA);

    key = await setupQueryNscp(nscp, "LUAScript", {
      "/modules": { LUAScript: "enabled", WEBServer: "enabled", [ALIAS]: "Scheduler" },
      "/settings/lua/scripts": { sched: script },
      // The template every schedule below inherits: the channel, a 5 s beat and
      // no jitter.
      [`${BASE}/schedules/default`]: {
        channel: CHANNEL,
        command: `${COMMAND} default`,
        interval: "5s",
        randomness: "0%",
        report: "all",
      },
      // The shorthand form: key = command, everything else inherited.
      [`${BASE}/schedules`]: { short: `${COMMAND} short` },
      [`${BASE}/schedules/explicit`]: { command: `${COMMAND} explicit` },
      [`${BASE}/schedules/every_1s`]: { command: `${COMMAND} 1s`, interval: "1s" },
      [`${BASE}/schedules/every_10s`]: { command: `${COMMAND} 10s`, interval: "10s" },
      // Randomness only ever brings a beat forward: the wait is drawn from
      // [interval * (1 - r), interval] and truncated to whole seconds, so 4 s
      // at 50% lands every 2 or 3 s.
      [`${BASE}/schedules/random`]: {
        command: `${COMMAND} rand`,
        interval: "4s",
        randomness: "50%",
      },
    });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("fires each schedule at a plausible rate for its interval", async () => {
    expect((await executeQuery(key, "sched_reset")).result).toBe(OK);
    const started = Date.now();
    await sleep(WINDOW_MS);
    const c = await counts();
    const seconds = (Date.now() - started) / 1000;

    // Expected ~= seconds / interval. The bands absorb a missed beat or two (a
    // CI stall) and the phase of the first beat inside the window.
    const band = (name: string, interval: number, slack: number) => {
      const expected = seconds / interval;
      const lo = Math.max(1, Math.floor(expected - slack));
      const hi = Math.ceil(expected + slack) + 1;
      const count = c[name] ?? 0;
      // One object, so a failure shows the schedule, its count and its band.
      expect({ name, count, lo, hi, inBand: count >= lo && count <= hi }).toMatchObject({
        inBand: true,
      });
    };
    band("1s", 1, 6);
    band("short", 5, 2);
    band("explicit", 5, 2);
    band("10s", 10, 1);
    // 2..3 s apart: about seconds/2.5 beats. The floor is what an unjittered
    // 4 s schedule would manage, less a slack beat for a busy runner.
    const rand = c.rand ?? 0;
    const randLo = Math.floor(seconds / 4);
    const randHi = Math.ceil(seconds / 2) + 1;
    expect({ rand, randLo, randHi, inBand: rand >= randLo && rand <= randHi }).toMatchObject({
      inBand: true,
    });

    // The ordering invariants, which hold however busy the machine is.
    expect(c["1s"]).toBeGreaterThan(c.rand ?? 0);
    expect(c.rand ?? 0).toBeGreaterThan(c.short ?? 0);
    expect(c.short ?? 0).toBeGreaterThanOrEqual(c["10s"] ?? 0);

    // `default` is the template, not a schedule of its own.
    expect(c.default).toBeUndefined();

    // Every result reached the channel, and the channel accepted it. A beat
    // in flight when the counters were read may not have landed yet.
    const fired = ["1s", "short", "explicit", "10s", "rand"].reduce((n, k) => n + (c[k] ?? 0), 0);
    expect(c.results).toBeGreaterThanOrEqual(fired - 5);
    expect(c.results).toBeLessThanOrEqual(fired);
    expect(nscp.capturedStdout()).not.toMatch(/Failed to submit/);
  });

  it("stops firing once the pool is set to zero threads", async () => {
    expect((await executeQuery(key, "sched_stop")).result).toBe(OK);
    // A reload asked for from inside a check is deferred to the core's own
    // scheduler, which can run it several seconds later. Wait until the 1 s
    // schedule has been quiet for three reads in a row...
    const deadline = Date.now() + 30_000;
    let before = await counts();
    for (let quiet = 0; quiet < 3 && Date.now() < deadline; ) {
      await sleep(1_000);
      const now = await counts();
      quiet = now["1s"] === before["1s"] ? quiet + 1 : 0;
      before = now;
    }
    // ...then require it to stay quiet.
    await sleep(5_000);
    const after = await counts();
    // With a 1 s schedule in the mix, a live scheduler would have fired ~5 times.
    expect(after["1s"] ?? 0).toBe(before["1s"] ?? 0);
    expect(after.rand ?? 0).toBe(before.rand ?? 0);
  });
});
