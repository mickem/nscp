/**
 * Covers the Scheduler's `run_schedules` command (issue #1450): after editing
 * nsclient.ini an operator wants to see the new result in Nagios *now* rather
 * than at the end of a five minute (or one hour) interval. `run_schedules`
 * executes the configured schedules on demand and submits their results on
 * their normal channel, so what the monitoring server receives is exactly what
 * the timer would have sent.
 *
 * Every schedule here uses a one hour interval and does not run on startup, so
 * any result arriving during the test can only have come from an explicit
 * `run_schedules` call.
 *
 * As in scheduler-run-on-startup.test.ts the results are observed through
 * GraphiteClient, whose carbon line protocol is plain TCP text: the "monitoring
 * server" is a local Node listener and the suite needs neither Docker nor the
 * network. The submitted alias is the schedule alias, which lands in the carbon
 * path via ${check_alias} and is what tells the schedules apart.
 */
import type { AddressInfo } from "net";
import * as net from "net";

import request from "supertest";

import { NscpInstance, REST_URL } from "@fixtures/index";

jest.setTimeout(120_000);

// Pin the status path so the assertions match on a known prefix rather than on
// whatever ${hostname} resolves to on the test machine.
const STATUS_PATH = "nscp.ondemand.${check_alias}.status";

describe("Scheduler run_schedules", () => {
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

  /** Carbon lines captured so far for one schedule alias. */
  function linesFor(alias: string, data = received): string[] {
    return data.split("\n").filter((l) => l.includes(`.${alias}.`));
  }

  /** Run a query over REST and hand back the JSON result payload. */
  async function runQuery(command: string, query = ""): Promise<Record<string, unknown>> {
    const response = await request(REST_URL)
      .get(`/api/v2/queries/${command}/commands/execute${query}`)
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
    return response.body as Record<string, unknown>;
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
        CheckHelpers: "enabled", // provides check_ok / check_critical
        Scheduler: "enabled",
        GraphiteClient: "enabled",
        WEBServer: "enabled", // used to trigger run_schedules
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
      // An interval far longer than the test and no startup run: nothing can
      // report unless run_schedules asks for it.
      "/settings/scheduler/schedules/default": {
        channel: "GRAPHITE",
        interval: "1h",
        report: "all", // check_ok returns OK, which is filtered out otherwise
      },
      "/settings/scheduler/schedules/firstcheck": { command: "check_ok" },
      "/settings/scheduler/schedules/secondcheck": { command: "check_ok" },
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

  it("reports nothing until asked", async () => {
    // Both schedules are an hour out, so a few seconds in the agent has sent
    // nothing at all.
    await new Promise((r) => setTimeout(r, 3_000));
    expect(received).toBe("");
  });

  it("runs a single named schedule", async () => {
    const result = await runQuery("run_schedules", "?schedule=firstcheck");
    expect(result.result).toEqual(0); // 0 = OK
    expect(result.lines).toEqual([
      expect.objectContaining({ message: expect.stringContaining("firstcheck") }),
    ]);

    const data = await waitForReceived((s) => linesFor("firstcheck", s).length > 0, 30_000);
    // Carbon line protocol: "<dotted.path> <value> <unix-timestamp>".
    expect(data).toMatch(/^nscp\.ondemand\.firstcheck\.status 0 \d+$/m);
    // Naming one schedule runs only that one.
    expect(linesFor("secondcheck", data)).toHaveLength(0);
  });

  it("runs every schedule when none is named", async () => {
    const before = linesFor("firstcheck").length;
    const result = await runQuery("run_schedules");
    expect(result.result).toEqual(0); // 0 = OK

    const data = await waitForReceived((s) => linesFor("secondcheck", s).length > 0, 30_000);
    expect(data).toMatch(/^nscp\.ondemand\.secondcheck\.status 0 \d+$/m);
    expect(linesFor("firstcheck", data)).toHaveLength(before + 1);
  });

  it("reports an unknown schedule instead of silently doing nothing", async () => {
    const result = await runQuery("run_schedules", "?schedule=nosuchcheck");
    expect(result.result).toEqual(3); // 3 = UNKNOWN
    expect(result.lines).toEqual([
      expect.objectContaining({
        message: expect.stringContaining("No such schedule: nosuchcheck"),
      }),
    ]);
  });

  it("does not disturb the regular schedule", async () => {
    // An on-demand run must not queue an extra timed instance: the schedules
    // are still an hour out, so nothing new arrives after the calls above.
    const before = received;
    await new Promise((r) => setTimeout(r, 3_000));
    expect(received).toBe(before);
  });
});
