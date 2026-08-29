/**
 * Covers the security property behind on-demand passive submission: the checks
 * run by `run_schedules` (Scheduler) and `check_and_forward` (CheckHelpers) are
 * permission-checked against the ORIGINAL caller, and — crucially — a denied
 * check submits NOTHING to the channel. The core answers a denied query as a
 * successful query with an UNKNOWN "Permission denied" payload, so without the
 * explicit denial check both commands would happily forward that denial text as
 * if it were the check's result, clobbering the last real result on the
 * monitoring server while telling the caller everything went fine.
 *
 * Also covers the run_schedules reentrancy guard: a schedule whose command is
 * run_schedules must be refused, not recursed into until the stack runs out.
 *
 * As in the other scheduler suites the "monitoring server" is a local Node
 * listener speaking GraphiteClient's plain-text carbon protocol.
 */
import type { AddressInfo } from "net";
import * as net from "net";

import request from "supertest";

import { NscpInstance, REST_URL } from "@fixtures/index";

jest.setTimeout(120_000);

// Pin the status path so the assertions match on a known prefix rather than on
// whatever ${hostname} resolves to on the test machine.
const STATUS_PATH = "nscp.gated.${check_alias}.status";

describe("passive submission permission forwarding", () => {
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

  /** Carbon lines captured so far for one schedule/forward alias. */
  function linesFor(alias: string, data = received): string[] {
    return data.split("\n").filter((l) => l.startsWith(`nscp.gated.${alias}.`));
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
        CheckHelpers: "enabled",
        Scheduler: "enabled",
        GraphiteClient: "enabled",
        WEBServer: "enabled",
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
      // Fail-closed policy: the web admin may run the two forwarding commands
      // and check_critical, but NOT check_ok. So the schedule/forward wrapping
      // check_critical goes through while the one wrapping check_ok is denied
      // even though the caller is allowed to ask for the run itself.
      "/settings/permissions": {
        enabled: "true",
        "log denials": "true",
      },
      "/settings/permissions/policies": {
        "WEBServer:admin":
          "Scheduler.run_schedules, CheckHelpers.check_and_forward, CheckHelpers.check_critical",
        // The scheduler's own timed runs stay attributed to Scheduler; the
        // intervals below are an hour out so this rule never actually fires
        // during the test, it is here for realism.
        Scheduler: "*",
      },
      // An interval far longer than the test and no startup run: nothing can
      // report unless run_schedules asks for it.
      "/settings/scheduler/schedules/default": {
        channel: "GRAPHITE",
        interval: "1h",
        report: "all",
      },
      "/settings/scheduler/schedules/allowedsched": { command: "check_critical" },
      "/settings/scheduler/schedules/deniedsched": { command: "check_ok" },
      "/settings/scheduler/schedules/recursivesched": { command: "run_schedules" },
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

  it("runs and submits a schedule whose check the caller may run", async () => {
    const result = await runQuery("run_schedules", "?schedule=allowedsched");
    expect(result.result).toEqual(0); // 0 = OK
    const data = await waitForReceived((s) => linesFor("allowedsched", s).length > 0, 30_000);
    // check_critical → status 2, proof the real check ran and was submitted.
    expect(data).toMatch(/^nscp\.gated\.allowedsched\.status 2 \d+$/m);
  });

  it("submits nothing when the schedule's check is denied for the caller", async () => {
    const result = await runQuery("run_schedules", "?schedule=deniedsched");
    expect(result.result).toEqual(3); // 3 = UNKNOWN
    expect(result.lines).toEqual([
      expect.objectContaining({ message: expect.stringContaining("Permission denied") }),
    ]);

    // The denial must not reach the channel either — give any stray
    // submission a moment to arrive, then assert there was none.
    await new Promise((r) => setTimeout(r, 3_000));
    expect(linesFor("deniedsched")).toEqual([]);
  });

  it("refuses a schedule that would recurse into run_schedules", async () => {
    const result = await runQuery("run_schedules", "?schedule=recursivesched");
    expect(result.result).toEqual(3); // 3 = UNKNOWN
    expect(result.lines).toEqual([
      expect.objectContaining({ message: expect.stringContaining("refusing to recurse") }),
    ]);
    expect(linesFor("recursivesched")).toEqual([]);

    // The refusal is an error, not a crash: the agent still answers.
    const again = await runQuery("run_schedules", "?schedule=allowedsched");
    expect(again.result).toEqual(0);
  });

  it("check_and_forward submits nothing when the wrapped check is denied", async () => {
    const result = await runQuery(
      "check_and_forward",
      "?command=check_ok&channel=GRAPHITE&alias=deniedfwd",
    );
    expect(result.result).toEqual(3); // 3 = UNKNOWN
    expect(result.lines).toEqual([
      expect.objectContaining({ message: expect.stringContaining("Permission denied") }),
    ]);

    await new Promise((r) => setTimeout(r, 3_000));
    expect(linesFor("deniedfwd")).toEqual([]);
  });

  it("check_and_forward still submits a check the caller may run", async () => {
    const result = await runQuery(
      "check_and_forward",
      "?command=check_critical&channel=GRAPHITE&alias=allowedfwd",
    );
    expect(result.result).toEqual(0); // 0 = OK
    const data = await waitForReceived((s) => linesFor("allowedfwd", s).length > 0, 30_000);
    expect(data).toMatch(/^nscp\.gated\.allowedfwd\.status 2 \d+$/m);
  });
});
