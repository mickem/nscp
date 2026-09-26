// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

/**
 * `nscp nsca install` — the command that points this agent's NSCA submissions
 * at a server, in place of three `nscp settings --set` calls.
 *
 * The thing worth pinning is *where the key lands*. NSCA encrypts the payload
 * with the shared secret rather than verifying it, so it can never use the
 * hashed `/settings/default/password` the web UI and check_nt verify inbound
 * callers against. The key belongs with the client target, and an NSCAServer
 * with no key of its own reads it from there.
 *
 * Runs the CLI against a scratch INI only: no server is started, and the
 * assertions read the file back.
 */
import * as fs from "fs";
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

/** Value of `key` under `[section]` in the INI, undefined when absent. */
function iniValue(file: string, section: string, key: string): string | undefined {
  let inSection = false;
  for (const raw of fs.readFileSync(file, "utf8").split(/\r?\n/)) {
    const line = raw.trim();
    if (line === "" || line.startsWith(";") || line.startsWith("#")) continue;
    if (line.startsWith("[")) {
      inSection = line === `[${section}]`;
      continue;
    }
    if (!inSection) continue;
    const eq = line.indexOf("=");
    if (eq < 0) continue;
    if (line.slice(0, eq).trim() === key) return line.slice(eq + 1).trim();
  }
  return undefined;
}

describe("nscp nsca install", () => {
  const TARGET = "/settings/NSCA/client/targets/default";
  const CLIENT = "/settings/NSCA/client";
  const SERVER = "/settings/NSCA/server";
  const SHARED = "/settings/default";
  let nscp: NscpInstance;

  beforeAll(() => {
    nscp = new NscpInstance();
  });

  const target = (key: string) => iniValue(nscp.settingsFile, TARGET, key);

  it("writes the target and enables the module", async () => {
    const r = await nscp.run([
      "nsca",
      "install",
      "--host",
      "nagios.example.com",
      "--password",
      "the-nsca-key",
      "--encryption",
      "aes256",
    ]);
    expect(r.all).toContain("nagios.example.com");
    expect(target("address")).toBe("nagios.example.com");
    expect(target("password")).toBe("the-nsca-key");
    expect(target("encryption")).toBe("aes256");
    expect(iniValue(nscp.settingsFile, "/modules", "NSCAClient")).toBe("enabled");
  });

  it("keeps the shared inbound password out of it", async () => {
    // The whole point of the separation: the key NSCA encrypts with must not
    // be the password the web UI and check_nt verify callers against.
    expect(iniValue(nscp.settingsFile, SHARED, "password")).toBeUndefined();
  });

  it("keeps what it was not given on a re-run", async () => {
    const r = await nscp.run(["nsca", "install", "--host", "other.example.com"]);
    expect(r.all).toContain("other.example.com");
    expect(target("address")).toBe("other.example.com");
    // Moving the server must not reset the cipher the daemon is configured
    // for, nor drop the key.
    expect(target("encryption")).toBe("aes256");
    expect(target("password")).toBe("the-nsca-key");
  });

  it("writes the port and the submitting host name when given", async () => {
    await nscp.run([
      "nsca",
      "install",
      "--host",
      "other.example.com",
      "--port",
      "15667",
      "--hostname",
      "as-nagios-knows-me",
    ]);
    expect(target("port")).toBe("15667");
    expect(iniValue(nscp.settingsFile, CLIENT, "hostname")).toBe("as-nagios-knows-me");
  });

  it("refuses without a host to submit to", async () => {
    const fresh = new NscpInstance();
    const r = await fresh.run(["nsca", "install", "--password", "the-nsca-key"], {
      allowFailure: true,
    });
    // A module that loads, registers its channel and drops every result is
    // worse than a command that refuses.
    expect(r.all).toContain("--host");
    expect(iniValue(fresh.settingsFile, TARGET, "address")).toBeUndefined();
  });

  it("warns about an empty key", async () => {
    const fresh = new NscpInstance();
    const r = await fresh.run(["nsca", "install", "--host", "nagios.example.com"]);
    // NSCA derives its key from the password, so an empty one is well known.
    expect(r.all).toContain("no password set");
    // And a fresh target still gets a usable cipher.
    expect(iniValue(fresh.settingsFile, TARGET, "encryption")).toBe("aes256");
  });

  it("says when an enabled NSCAServer will share the key", async () => {
    const fresh = new NscpInstance();
    await fresh.run(["settings", "--path", "/modules", "--key", "NSCAServer", "--set", "enabled"]);
    const r = await fresh.run([
      "nsca",
      "install",
      "--host",
      "nagios.example.com",
      "--password",
      "the-nsca-key",
    ]);
    expect(r.all).toContain("NSCAServer is enabled with no key of its own");
  });

  it("stays quiet when NSCAServer has a key of its own", async () => {
    const fresh = new NscpInstance();
    await fresh.run(["settings", "--path", "/modules", "--key", "NSCAServer", "--set", "enabled"]);
    await fresh.run([
      "settings",
      "--path",
      SERVER,
      "--key",
      "password",
      "--set",
      "a-different-key",
    ]);
    const r = await fresh.run([
      "nsca",
      "install",
      "--host",
      "nagios.example.com",
      "--password",
      "the-nsca-key",
    ]);
    expect(r.all).not.toContain("NSCAServer is enabled with no key");
  });
});
