/**
 * Covers CheckHelpers' `check_and_forward`: run a check and submit its result
 * as a passive check, without waiting for a schedule to come around.
 *
 * This used to hand the raw query response straight to the core's channel API,
 * which parses what it gets as a SubmitRequestMessage. The two messages are not
 * wire compatible (the query payload is field 2, the submit payload field 3),
 * so the channel received a message with no payloads at all: the check ran, the
 * command answered "Message submitted" and nothing whatsoever reached the
 * monitoring server. The assertions below are on what the receiving end
 * actually gets, which is the part that was broken.
 *
 * As in the scheduler suites the results are observed through GraphiteClient,
 * whose carbon line protocol is plain TCP text, so the "monitoring server" is a
 * local Node listener and the suite needs neither Docker nor the network.
 */
import type { AddressInfo } from "net";
import * as net from "net";

import request from "supertest";

import { NscpInstance, REST_URL } from "@fixtures/index";

jest.setTimeout(120_000);

// Pin the paths so the assertions match on a known prefix rather than on
// whatever ${hostname} resolves to on the test machine.
const STATUS_PATH = "nscp.forward.${check_alias}.status";
const PERF_PATH = "nscp.forward.${check_alias}.${perf_alias}";

describe("CheckHelpers check_and_forward", () => {
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
        "perf path": PERF_PATH,
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

  it("forwards the result of a check to a channel", async () => {
    const result = await runQuery(
      "check_and_forward",
      "?command=check_ok&channel=GRAPHITE&alias=forwarded",
    );
    expect(result.result).toEqual(0); // 0 = OK
    expect(result.lines).toEqual([
      expect.objectContaining({ message: expect.stringContaining("Message submitted") }),
    ]);

    // Carbon line protocol: "<dotted.path> <value> <unix-timestamp>".
    const data = await waitForReceived((s) => s.includes("forwarded"), 30_000);
    expect(data).toMatch(/^nscp\.forward\.forwarded\.status 0 \d+$/m);
  });

  it("forwards the status of a failing check", async () => {
    await runQuery(
      "check_and_forward",
      "?command=check_critical&channel=GRAPHITE&alias=failing&arguments=message%3Ddisk-is-full",
    );
    const data = await waitForReceived((s) => s.includes("failing"), 30_000);
    // 2 = CRITICAL: the status the wrapped check returned, not a fixed OK.
    expect(data).toMatch(/^nscp\.forward\.failing\.status 2 \d+$/m);
  });

  it("names the result after the command when no alias is given", async () => {
    await runQuery("check_and_forward", "?command=check_ok&channel=GRAPHITE");
    const data = await waitForReceived((s) => s.includes("check_ok"), 30_000);
    expect(data).toMatch(/^nscp\.forward\.check_ok\.status 0 \d+$/m);
  });

  it("still accepts the legacy target= spelling of channel", async () => {
    await runQuery("check_and_forward", "?command=check_ok&target=GRAPHITE&alias=legacyspelling");
    const data = await waitForReceived((s) => s.includes("legacyspelling"), 30_000);
    expect(data).toMatch(/^nscp\.forward\.legacyspelling\.status 0 \d+$/m);
  });

  it("fails when the channel has no handler", async () => {
    const result = await runQuery(
      "check_and_forward",
      "?command=check_ok&channel=NOSUCHCHANNEL&alias=nowhere",
    );
    expect(result.result).toEqual(3); // 3 = UNKNOWN
    expect(result.lines).toEqual([
      expect.objectContaining({ message: expect.stringContaining("NOSUCHCHANNEL") }),
    ]);
  });

  it("requires a command", async () => {
    const result = await runQuery("check_and_forward", "?channel=GRAPHITE");
    expect(result.result).toEqual(3); // 3 = UNKNOWN
    expect(result.lines).toEqual([
      expect.objectContaining({ message: expect.stringContaining("Missing command") }),
    ]);
  });
});
