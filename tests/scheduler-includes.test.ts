/**
 * Covers issue #636: schedules defined in a file pulled in via `[/includes]`
 * lost their command — the Scheduler enumerated the keys from the included
 * file but every value read came back empty, so it logged
 * "Adding scheduled item: ... command: , ..." and submitted nothing useful.
 *
 * The setup mirrors the report: the main ini includes a *directory*, and the
 * schedules live in a file inside it (that chains two child settings stores:
 * main -> directory -> file). Three schedules are defined:
 *
 *   - localcheck    — in the main ini (control: this always worked)
 *   - includedcheck — key=value in the included file (the #636 case)
 *   - sectioncheck  — its own section in the included file
 *
 * All inherit `run on startup` from the default schedule, so each must report
 * exactly once within seconds of boot. Results travel over GraphiteClient's
 * carbon line protocol to a local TCP listener (no Docker, no network). A
 * schedule whose command was lost reports UNKNOWN (status 3) — or nothing at
 * all — so asserting `status 0` proves the command survived the include.
 */
import type { AddressInfo } from "net";
import * as net from "net";

import * as fs from "fs";
import * as path from "path";

import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

// Pin the status path so assertions match on a known prefix rather than on
// whatever ${hostname} resolves to on the test machine.
const STATUS_PATH = "nscp.inctest.${check_alias}.status";

describe("Scheduler schedules from [/includes] files", () => {
  let nscp: NscpInstance;
  let server: net.Server;
  let received = "";

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

    // The included directory, as in the issue's /etc/nsclient/ini/. Both the
    // one-liner and the section variant live here; neither exists in the main
    // ini, so a value that fails to resolve across the include chain cannot
    // be masked by local configuration.
    const confDir = nscp.scratch("conf.d");
    fs.writeFileSync(
      path.join(confDir, "schedules.ini"),
      [
        "[/settings/scheduler/schedules]",
        "includedcheck = check_ok",
        "",
        "[/settings/scheduler/schedules/sectioncheck]",
        "command = check_ok",
        "",
      ].join("\n"),
    );

    await nscp.configure({
      "/modules": {
        CheckHelpers: "enabled", // provides check_ok
        Scheduler: "enabled",
        GraphiteClient: "enabled",
      },
      // The trailing separator marks the include as a directory, like the
      // `otherfiles=/etc/nsclient/ini/` line in the issue.
      "/includes": { otherfiles: confDir + path.sep },
      "/settings/graphite/client/targets/default": {
        address: `127.0.0.1:${port}`,
        "status path": STATUS_PATH,
      },
      // An interval far longer than the test plus `run on startup`: every
      // schedule reports once right after boot and then stays silent, so a
      // captured result can only mean the schedule was wired up correctly.
      "/settings/scheduler/schedules/default": {
        channel: "GRAPHITE",
        interval: "1h",
        report: "all", // check_ok returns OK, which is filtered out otherwise
        "run on startup": "true",
      },
      // The control: same shape as includedcheck but defined locally.
      "/settings/scheduler/schedules": { localcheck: "check_ok" },
    });

    nscp.start();
  });

  afterAll(async () => {
    await nscp?.stop();
    await new Promise<void>((resolve) => server?.close(() => resolve()));
  });

  it("runs a schedule defined in the main ini (control)", async () => {
    const data = await waitForReceived((s) => linesFor("localcheck", s).length > 0, 60_000);
    // Carbon line protocol: "<dotted.path> <value> <unix-timestamp>".
    expect(data).toMatch(/^nscp\.inctest\.localcheck\.status 0 \d+$/m);
  });

  it("resolves the command of a key=value schedule from an included directory (#636)", async () => {
    const data = await waitForReceived((s) => linesFor("includedcheck", s).length > 0, 60_000);
    expect(data).toMatch(/^nscp\.inctest\.includedcheck\.status 0 \d+$/m);
  });

  it("resolves a section-based schedule from an included directory", async () => {
    const data = await waitForReceived((s) => linesFor("sectioncheck", s).length > 0, 60_000);
    expect(data).toMatch(/^nscp\.inctest\.sectioncheck\.status 0 \d+$/m);
  });
});
