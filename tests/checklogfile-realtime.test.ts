/**
 * Covers `run on startup` for real-time filters (issue #584): a real-time
 * check normally submits nothing until its file changes or `maximum age`
 * expires, so a destination like SimpleCache stays empty right after the
 * agent starts and `check_cache` fails with "Entry not found" until the
 * first trigger. With `run on startup` the filter submits its "empty
 * message" (OK) as soon as the module is up, priming the destination.
 *
 * The suite boots one agent with CheckLogFile real-time filters routed to
 * SimpleCache and observes the cache over REST via check_cache:
 *
 *   - rt_explicit — run on startup = true       → primed within seconds
 *   - rt_inherit  — inherits true from default  → primed within seconds
 *   - rt_optout   — run on startup = false      → must stay absent
 *
 * `maximum age` is disabled everywhere so the only way an entry can appear
 * during the test is the startup submission (or a real file change, which
 * the last test triggers deliberately to prove the primed filter still
 * reacts to live data).
 */
import * as fs from "fs";
import * as os from "os";
import * as path from "path";

import {
  NscpInstance,
  OK,
  UNKNOWN,
  executeQuery,
  messageOf,
  pollQuery,
  setupQueryNscp,
} from "@fixtures/index";

jest.setTimeout(300_000);

describe("CheckLogFile real-time run on startup", () => {
  let nscp: NscpInstance;
  let key: string;
  let scratch: string;
  let explicitLog: string;
  let inheritLog: string;
  let optoutLog: string;

  function checkCache(cacheKey: string) {
    return executeQuery(key, "check_cache", { key: cacheKey });
  }

  beforeAll(async () => {
    scratch = fs.mkdtempSync(path.join(os.tmpdir(), "checklogfile-rt-"));
    explicitLog = path.join(scratch, "explicit.log");
    inheritLog = path.join(scratch, "inherit.log");
    optoutLog = path.join(scratch, "optout.log");
    for (const f of [explicitLog, inheritLog, optoutLog]) fs.writeFileSync(f, "");

    nscp = new NscpInstance();
    key = await setupQueryNscp(nscp, "CheckLogFile", {
      "/modules": {
        CheckLogFile: "enabled",
        SimpleCache: "enabled",
        WEBServer: "enabled",
      },
      "/settings/logfile/real-time": { enabled: "true" },
      // The template: everything below inherits the destination, the filter
      // and — unless overridden — `run on startup`. `maximum age` is off so
      // no periodic-ok submission can prime an entry behind the test's back.
      "/settings/logfile/real-time/checks/default": {
        destination: "cache",
        filter: "column1 like 'ERROR'",
        "maximum age": "false",
        "run on startup": "true",
      },
      "/settings/logfile/real-time/checks/rt_explicit": {
        file: explicitLog,
        "maximum age": "false",
        "empty message": "primed explicit",
        "run on startup": "true",
      },
      "/settings/logfile/real-time/checks/rt_inherit": {
        file: inheritLog,
        "maximum age": "false",
        "empty message": "primed inherit",
      },
      // The control: an explicit override wins over the inherited true, so
      // this filter keeps the old behaviour and submits nothing at boot.
      "/settings/logfile/real-time/checks/rt_optout": {
        file: optoutLog,
        "maximum age": "false",
        "empty message": "primed optout",
        "run on startup": "false",
      },
    });
  });

  afterAll(async () => {
    await nscp?.stop();
    // beforeAll can fail before `scratch` is assigned; don't let teardown
    // throw a secondary error that masks the original failure.
    if (scratch) fs.rmSync(scratch, { recursive: true, force: true });
  });

  it("primes the cache at startup when run on startup is set", async () => {
    const res = await pollQuery(key, "check_cache", { key: "rt_explicit" }, (q) => q.result === OK);
    expect(res.result).toBe(OK);
    expect(messageOf(res)).toBe("primed explicit");
  });

  it("inherits run on startup from the default filter", async () => {
    const res = await pollQuery(key, "check_cache", { key: "rt_inherit" }, (q) => q.result === OK);
    expect(res.result).toBe(OK);
    expect(messageOf(res)).toBe("primed inherit");
  });

  it("honours an explicit run on startup = false override", async () => {
    // The primed entries above prove the startup pass has run and completed,
    // so an entry for the opted-out filter would have to exist by now too.
    const res = await checkCache("rt_optout");
    expect(res.result).toBe(UNKNOWN);
    expect(messageOf(res)).toMatch(/Entry not found/i);
  });

  it("still reacts to live file changes after the startup priming", async () => {
    // The primed entry must be replaced by a real result once data arrives,
    // proving the startup submission did not consume or break the trigger
    // path of the filter.
    fs.appendFileSync(explicitLog, "ERROR live\n");
    const res = await pollQuery(key, "check_cache", { key: "rt_explicit" }, (q) =>
      messageOf(q).includes("ERROR live"),
    );
    expect(messageOf(res)).toContain("ERROR live");
  });
});
