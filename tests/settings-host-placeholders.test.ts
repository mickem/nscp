/**
 * End-to-end test of host name placeholder expansion in the settings
 * subsystem (issue #458): one fleet-wide configuration, served over http,
 * gives every agent its own per-host config file and check script.
 *
 * The local bootstrap ini includes a fleet-wide ini from a fake config
 * server. The fleet ini attaches a per-host configuration file — target path
 * and source url both named with ${host} — and includes that file. The
 * per-host file in turn attaches a per-host check script and defines an
 * external script command that runs it.
 *
 * Convergence deliberately takes two boots: the http store resolves its
 * include chain from the freshly downloaded config before it fetches the
 * attachments, so the per-host file dropped by boot one is only picked up as
 * an include on boot two — which then also sees the script attachment the
 * per-host file declares, downloads it, and can run the command.
 *
 * Before the fix, ${host} in an attachment target or an included file name
 * fell through to the path manager, where an unknown token silently expands
 * to the installation path: the attachment was written under a mangled name
 * and the include never matched anything.
 */
import http from "http";
import { AddressInfo } from "net";
import fs from "fs";
import os from "os";
import path from "path";
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(180_000);

const onWindows = process.platform === "win32";
// ${host} expands to the OS host name up to the first dot, case preserved —
// the same gethostname() node's os.hostname() reports.
const host = os.hostname().split(".")[0];

describe("settings host name placeholders (issue #458)", () => {
  let nscp: NscpInstance;
  let workDir: string;
  let server: http.Server;
  let baseUrl: string;
  const requests: string[] = [];

  const ext = onWindows ? "bat" : "sh";
  const marker = `fleet-greeting-for-${host}`;
  const scriptBody = onWindows
    ? `@echo off\r\necho OK: ${marker}\r\n`
    : `#!/bin/sh\necho "OK: ${marker}"\n`;

  const perHostFile = () => path.join(workDir, `${host}-nsclient.ini`);
  const scriptFile = () => path.join(workDir, `${host}-hello.${ext}`);

  // Server-side content; built in beforeAll once baseUrl/workDir are known.
  let fleetIni: string;
  let perHostIni: string;

  beforeAll(async () => {
    workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-hostcfg-"));
    // ${shared-path} is where the fleet config drops the per-host files, so
    // point it into the sandbox.
    nscp = new NscpInstance({ workDir, pathOverrides: { "shared-path": workDir } });

    server = http.createServer((req, res) => {
      const url = decodeURIComponent(req.url ?? "/");
      requests.push(url);
      const serve = (body: string) => {
        res.writeHead(200, { "Content-Type": "text/plain" });
        res.end(body);
      };
      if (url === "/conf/fleet.ini") return serve(fleetIni);
      if (url === `/hosts/${host}-nsclient.ini`) return serve(perHostIni);
      if (url === `/scripts/${host}-hello.${ext}`) return serve(scriptBody);
      res.writeHead(404);
      res.end("not found");
    });
    await new Promise<void>((resolve) => server.listen(0, "127.0.0.1", resolve));
    baseUrl = `http://127.0.0.1:${(server.address() as AddressInfo).port}`;

    // The fleet-wide file, identical for every host in the fleet. Attachment
    // target and include mix both placeholder vocabularies: the host pass has
    // to resolve ${host} and leave ${shared-path} to the path manager.
    fleetIni = [
      "[/attachments]",
      "${shared-path}/${host}-nsclient.ini = " + baseUrl + "/hosts/${host}-nsclient.ini",
      "",
      "[/includes]",
      "client = ini://${shared-path}/${host}-nsclient.ini",
      "",
    ].join("\n");

    // The per-host file: attaches this host's check script and wires it up as
    // an external script command. The command uses the resolved absolute path
    // (unquoted: a temp dir has no spaces on any platform we run on).
    const runner = onWindows ? `cmd /c ${scriptFile()}` : `/bin/sh ${scriptFile()}`;
    perHostIni = [
      "[/attachments]",
      "${shared-path}/${host}-hello." + ext + " = " + baseUrl + "/scripts/${host}-hello." + ext,
      "",
      "[/settings/external scripts/scripts]",
      `check_fleet_host = ${runner}`,
      "",
    ].join("\n");

    // Local bootstrap: everything else comes from the config server.
    fs.writeFileSync(nscp.settingsFile, `[/includes]\nfleet = ${baseUrl}/conf/fleet.ini\n`);
  });

  afterAll(async () => {
    await new Promise<void>((resolve) => server.close(() => resolve()));
  });

  /** One-shot boot + query over the client-query path (no web server needed). */
  async function queryFleetHost() {
    return nscp.run(
      ["client", "--module", "CheckExternalScripts", "--boot", "--query", "check_fleet_host"],
      { allowFailure: true },
    );
  }

  it("first boot downloads the per-host config under this host's name", async () => {
    // The query itself is expected to fail on this boot (the include chain
    // was resolved before the attachment landed); it is only run to boot the
    // settings subsystem once.
    await queryFleetHost();

    // The attachment target ${shared-path}/${host}-nsclient.ini resolved both
    // kinds of token: the file is here, byte for byte, under OUR host name...
    expect(fs.readFileSync(perHostFile(), "utf8")).toBe(perHostIni);

    // ...and it is the only *-nsclient.ini in the folder. Before the fix the
    // unresolved ${host} token silently expanded to the installation path and
    // the attachment was written under a mangled name instead.
    const strays = fs
      .readdirSync(workDir)
      .filter((f) => f.endsWith("-nsclient.ini") && f !== `${host}-nsclient.ini`);
    expect(strays).toEqual([]);

    // The attachment source url asked the server for this host's file by name,
    // and no request ever went out with an unexpanded placeholder.
    expect(requests).toContain(`/hosts/${host}-nsclient.ini`);
    expect(requests.filter((u) => u.includes("${"))).toEqual([]);
  });

  it("second boot includes the per-host config, downloads its script and runs it", async () => {
    const r = await queryFleetHost();
    const out = r.all ?? `${r.stdout}\n${r.stderr}`;

    // The included per-host file is live: the command it defines resolved,
    // the script it attaches was fetched from the server and executed.
    // (Client-query output is the raw message, no status-word prefix.)
    expect(out).toContain(marker);
    expect(r.exitCode).toBe(0);

    expect(requests).toContain(`/scripts/${host}-hello.${ext}`);
    expect(fs.readFileSync(scriptFile(), "utf8")).toBe(scriptBody);
  });
});
