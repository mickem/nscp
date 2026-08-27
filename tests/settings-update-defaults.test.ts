/**
 * Verifies the contract of `nscp settings --update --add-defaults` (issue #542):
 * filling in defaults must not fabricate configuration. Around 0.5.2 the
 * generated file was polluted with placeholder values rendered as `UNKNOWN`
 * and with sample/template sections, which every deployment then had to clean
 * up by hand. Today defaults come from the settings registry, which excludes
 * sample keys and advanced keys — this suite pins that behaviour.
 *
 * Each case runs one-shot `nscp settings ...` invocations against a throwaway
 * settings file and inspects the resulting ini. No server/port/docker needed.
 *
 * The modules used (CheckHelpers, Scheduler) exist on both Linux and Windows,
 * so the suite runs on both platforms.
 */
import * as fs from "fs";
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

/** Parse an nsclient.ini into {section -> {key -> value}} (comments dropped). */
function parseIni(iniPath: string): Map<string, Map<string, string>> {
  const sections = new Map<string, Map<string, string>>();
  let current: Map<string, string> | undefined;
  for (const raw of fs.readFileSync(iniPath, "utf8").split(/\r?\n/)) {
    const line = raw.trim();
    if (line === "" || line.startsWith(";") || line.startsWith("#")) continue;
    const header = line.match(/^\[(.+)\]$/);
    if (header) {
      const name = header[1];
      current = sections.get(name) ?? new Map<string, string>();
      sections.set(name, current);
      continue;
    }
    if (!current) continue;
    const eq = line.indexOf("=");
    if (eq < 0) continue;
    current.set(line.slice(0, eq).trim(), line.slice(eq + 1).trim());
  }
  return sections;
}

/** The module keys set to `enabled` in the [/modules] section. */
function enabledModules(sections: Map<string, Map<string, string>>): Set<string> {
  const out = new Set<string>();
  for (const [key, value] of sections.get("/modules") ?? []) {
    if (value === "enabled") out.add(key);
  }
  return out;
}

/** Every `path.key=value` whose value matches `pred`, for failure messages. */
function valuesWhere(
  sections: Map<string, Map<string, string>>,
  pred: (value: string) => boolean,
): string[] {
  const hits: string[] = [];
  for (const [section, keys] of sections) {
    for (const [key, value] of keys) {
      if (pred(value)) hits.push(`${section}.${key}=${value}`);
    }
  }
  return hits;
}

describe("settings --update --add-defaults", () => {
  it("adds registered defaults without fabricating placeholders or samples", async () => {
    const nscp = new NscpInstance();
    await nscp.run(["settings", "--activate-module", "CheckHelpers", "Scheduler"]);

    const r = await nscp.run(["settings", "--update", "--add-defaults"]);
    expect(r.exitCode).toBe(0);
    const sections = parseIni(nscp.settingsFile);

    // A real registered default is filled in: [/settings/log] level = info.
    expect(sections.get("/settings/log")?.get("level")).toBe("info");

    // The 0.5.2-era failure mode: keys without a usable default were written
    // as the literal placeholder UNKNOWN and broke the agent until edited.
    expect(valuesWhere(sections, (v) => v === "UNKNOWN")).toEqual([]);

    // Sample/template objects (Scheduler's sample schedule, external script
    // samples, ...) are registered as samples and must stay out of the file
    // unless explicitly asked for.
    const sampleSections = [...sections.keys()].filter((s) => s.endsWith("/sample"));
    expect(sampleSections).toEqual([]);

    // Adding defaults must not grow the module list: exactly the modules that
    // were active before, nothing force-enabled.
    expect(enabledModules(sections)).toEqual(new Set(["CheckHelpers", "Scheduler"]));
  });

  it("keeps a value the operator set over the registered default", async () => {
    const nscp = new NscpInstance();
    await nscp.run(["settings", "--activate-module", "CheckHelpers"]);
    await nscp.run(["settings", "--path", "/settings/log", "--key", "level", "--set", "debug"]);

    const r = await nscp.run(["settings", "--update", "--add-defaults"]);
    expect(r.exitCode).toBe(0);
    expect(parseIni(nscp.settingsFile).get("/settings/log")?.get("level")).toBe("debug");
  });

  it("--remove-defaults strips the default-valued keys again", async () => {
    const nscp = new NscpInstance();
    await nscp.run(["settings", "--activate-module", "CheckHelpers"]);
    await nscp.run(["settings", "--update", "--add-defaults"]);
    expect(parseIni(nscp.settingsFile).get("/settings/log")?.get("level")).toBe("info");

    const r = await nscp.run(["settings", "--update", "--remove-defaults"]);
    expect(r.exitCode).toBe(0);
    expect(parseIni(nscp.settingsFile).get("/settings/log")?.get("level")).toBeUndefined();
  });

  it("--use-samples writes the sample objects, and --remove-defaults strips them again", async () => {
    const nscp = new NscpInstance();
    await nscp.run(["settings", "--activate-module", "CheckHelpers", "Scheduler"]);

    const r = await nscp.run(["settings", "--update", "--add-defaults", "--use-samples"]);
    expect(r.exitCode).toBe(0);
    let sections = parseIni(nscp.settingsFile);

    // The Scheduler's sample schedule is materialized as a starting point...
    expect([...sections.keys()]).toContain("/settings/scheduler/schedules/sample");
    // ...with real defaults, not placeholders.
    expect(valuesWhere(sections, (v) => v === "UNKNOWN")).toEqual([]);

    // remove-defaults is the inverse: the untouched sample sections disappear.
    const rm = await nscp.run(["settings", "--update", "--remove-defaults"]);
    expect(rm.exitCode).toBe(0);
    sections = parseIni(nscp.settingsFile);
    expect([...sections.keys()].filter((s) => s.endsWith("/sample"))).toEqual([]);
  });

  it("the deprecated --generate spelling honours the same contract", async () => {
    const nscp = new NscpInstance();
    await nscp.run(["settings", "--activate-module", "CheckHelpers", "Scheduler"]);

    const r = await nscp.run(["settings", "--generate", "--add-defaults"]);
    expect(r.exitCode).toBe(0);
    const sections = parseIni(nscp.settingsFile);

    expect(sections.get("/settings/log")?.get("level")).toBe("info");
    expect(valuesWhere(sections, (v) => v === "UNKNOWN")).toEqual([]);
    expect([...sections.keys()].filter((s) => s.endsWith("/sample"))).toEqual([]);
    expect(enabledModules(sections)).toEqual(new Set(["CheckHelpers", "Scheduler"]));
  });
});
