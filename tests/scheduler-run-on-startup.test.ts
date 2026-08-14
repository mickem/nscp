/**
 * Covers the Scheduler's "run on startup" option (issue #392): a schedule with
 * a long interval normally reports nothing until the first interval elapses,
 * which leaves the monitoring server with stale data for hours after a reboot
 * or a config change. With `run on startup` the check runs once as soon as the
 * agent is up and then follows its normal schedule.
 *
 * Both schedules here use a one hour interval, so *any* result arriving during
 * the test can only have come from a startup run:
 *
 *   - startupcheck  — run on startup = true  → must report within seconds
 *   - intervalcheck — plain 1h schedule      → must stay silent
 *
 * Results are observed through GraphiteClient, whose carbon line protocol is
 * plain TCP text, so the "monitoring server" is a local Node listener and the
 * suite needs neither Docker nor the network. The submitted result's alias is
 * the schedule alias, which lands in the carbon path via `${check_alias}` and
 * is what tells the two schedules apart.
 */
import type { AddressInfo } from "net";
import * as net from "net";

import request from "supertest";

import { NscpInstance, REST_URL } from "@fixtures/index";

jest.setTimeout(120_000);

// Pin the status path so the assertions match on a known prefix rather than on
// whatever ${hostname} resolves to on the test machine.
const STATUS_PATH = "nscp.startuptest.${check_alias}.status";

describe("Scheduler run on startup", () => {
  let nscp: NscpInstance;
  let server: net.Server;
  let received = "";
  let key = "";

  /** Poll the captured carbon stream until `predicate` holds. */
  async function waitForReceived(
    predicate: (s: string) => boolean,
    timeoutMs: number,
  ): Promise<string> {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      if (predicate(received)) return received;
      await new Promise((r) => setTimeout(r, 250));
    }
    return received; // hand back whatever we have so expect() shows a useful diff
  }

  beforeAll(async () => {
    const port = await new Promise<number>((resolve) => {
      server = net.createServer((sock) => {
        sock.on("error", () => {});
        sock.on("data", (chunk) => {
          received += chunk.toString();
        });
      });
      server.listen(0, "127.0.0.1", () => resolve((server.address() as AddressInfo).port));
    });

    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        CheckHelpers: "enabled", // provides check_ok
        Scheduler: "enabled",
        GraphiteClient: "enabled",
        WEBServer: "enabled", // only used to trigger the reload below
      },
      "/settings/default": {
        password: "default-password",
        "allowed hosts": "127.0.0.1,::1",
      },
      "/settings/WEB/server/roles": { full: "*" },
      "/settings/WEB/server/users/admin": {
        role: "full",
        password: "default-password",
      },
      "/settings/graphite/client/targets/default": {
        address: `127.0.0.1:${port}`,
        "status path": STATUS_PATH,
      },
      // Shared by the schedules below: an interval far longer than the test, so
      // nothing can report unless it was run at startup. `run on startup` is
      // set here to also cover the "turn it on for everything" case - the
      // schedules inherit it from this template.
      "/settings/scheduler/schedules/default": {
        channel: "GRAPHITE",
        interval: "1h",
        report: "all", // check_ok returns OK, which is filtered out otherwise
        "run on startup": "true",
      },
      "/settings/scheduler/schedules/inheritcheck": {
        command: "check_ok",
      },
      "/settings/scheduler/schedules/explicitcheck": {
        command: "check_ok",
        "run on startup": "true",
      },
      // The control: an explicit override wins over the inherited value, so
      // this one keeps its regular first run an hour out.
      "/settings/scheduler/schedules/silentcheck": {
        command: "check_ok",
        "run on startup": "false",
      },
    });

    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
    const login = await request(REST_URL)
      .get("/api/v2/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200);
    key = login.body.key as string;
  });

  afterAll(async () => {
    await nscp?.stop();
    await new Promise<void>((resolve) => server?.close(() => resolve()));
  });

  /** Carbon lines captured so far for one schedule alias. */
  function linesFor(alias: string, data = received): string[] {
    return data.split("\n").filter((l) => l.includes(`.${alias}.`));
  }

  it("runs a run-on-startup schedule without waiting for its interval", async () => {
    const data = await waitForReceived((s) => linesFor("explicitcheck", s).length > 0, 60_000);
    // Carbon line protocol: "<dotted.path> <value> <unix-timestamp>".
    expect(data).toMatch(/^nscp\.startuptest\.explicitcheck\.status 0 \d+$/m);
  });

  it("inherits the option from the default schedule", async () => {
    const data = await waitForReceived((s) => linesFor("inheritcheck", s).length > 0, 60_000);
    expect(data).toMatch(/^nscp\.startuptest\.inheritcheck\.status 0 \d+$/m);
  });

  it("runs each schedule once and honours an explicit override", async () => {
    // Settle first: a startup run queued twice (once by the module load, once
    // by the start hook) would show up as a second line right away.
    await new Promise((r) => setTimeout(r, 3_000));
    expect(linesFor("explicitcheck")).toHaveLength(1);
    expect(linesFor("inheritcheck")).toHaveLength(1);
    // `run on startup = false` overrides the inherited true, so this schedule
    // keeps its normal first run an hour out and has reported nothing.
    expect(linesFor("silentcheck")).toHaveLength(0);
  });

  it("runs them again after a configuration reload", async () => {
    // "Config changed, tell me the new status now" is the other half of #392 -
    // and a reload never goes through the plugin start hook, so this is a
    // separate code path from the boot case above.
    const before = linesFor("explicitcheck").length;
    await request(REST_URL)
      .post("/api/v2/settings/command")
      .set("Authorization", `Bearer ${key}`)
      .send({ command: "reload" })
      .trustLocalhost(true)
      .expect(200);

    const data = await waitForReceived((s) => linesFor("explicitcheck", s).length > before, 60_000);
    // Exactly one more: the schedules from before the reload are dropped, so
    // the reloaded agent has a single startup run, not one per reload cycle.
    expect(linesFor("explicitcheck", data)).toHaveLength(before + 1);
    expect(linesFor("silentcheck", data)).toHaveLength(0);
  });
});
