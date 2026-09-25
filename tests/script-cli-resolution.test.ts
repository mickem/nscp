/**
 * What the `ext-scr` and `lua` CLIs record, and what those recordings resolve
 * to afterwards.
 *
 * `script-folders.test.ts` covers the loader: given a value already in
 * nsclient.ini, where is the script found. This covers the other half — what
 * the CLI *writes* into nsclient.ini in the first place, which is a separate
 * decision and not always the value you typed.
 *
 * The interesting part is the gap between the two. Both CLIs accept a value if
 * they can find it *right now*, from the directory they happen to be run in,
 * and then record it verbatim. So `nscp lua add --script scripts/lua/c.lua`
 * succeeds when run from the installation directory and writes a value that the
 * service — which runs from somewhere else — cannot resolve. The CLI says
 * "Added", and the check never works.
 *
 * Each table is printed as it runs, so the resolution is readable in the test
 * output rather than only implied by assertions.
 */
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { NscpInstance, describeOnLinux } from "@fixtures/index";

jest.setTimeout(240_000);

/** One CLI invocation plus what became of it. */
interface Resolution {
  /** What was typed after --script. */
  typed: string;
  /** The CLI's own one-line answer. */
  answered: string;
  /** The value that landed in nsclient.ini, or undefined if nothing did. */
  recorded?: string;
  /** Whether it works when the agent runs from a different directory. */
  worksElsewhere: boolean;
}

function readSection(ini: string, section: string): { alias: string; value: string } | undefined {
  const body = ini.split(`[${section}]`)[1];
  if (body === undefined) return undefined;
  for (const raw of body.split("\n")) {
    const line = raw.trim();
    if (!line || line.startsWith(";") || line.startsWith("[")) {
      if (line.startsWith("[")) break;
      continue;
    }
    const at = line.indexOf("=");
    if (at < 0) continue;
    return { alias: line.slice(0, at).trim(), value: line.slice(at + 1).trim() };
  }
  return undefined;
}

/** Last non-empty line that is not the dev-build modules-folder warning. */
function answer(output: string): string {
  const lines = output
    .split("\n")
    .map((l) => l.trim())
    .filter((l) => l && !/Modules folder .* not found/.test(l) && !/Duplicate command/.test(l));
  return lines[lines.length - 1] ?? "";
}

function table(title: string, rows: Resolution[], scrub: (s: string) => string): void {
  const out = [`\n  ${title}`, `  ${"-".repeat(title.length)}`];
  for (const r of rows) {
    out.push(
      `  typed      ${scrub(r.typed)}\n` +
        `    answered   ${scrub(r.answered)}\n` +
        `    recorded   ${r.recorded === undefined ? "(nothing)" : scrub(r.recorded)}\n` +
        `    elsewhere  ${r.worksElsewhere ? "resolves" : "DOES NOT RESOLVE"}`,
    );
  }
  console.log(out.join("\n"));
}

describe("script CLI resolution", () => {
  // --- lua ------------------------------------------------------------------
  describe("nscp lua add", () => {
    let root: string;
    let neutral: string;
    let available = false;
    const scrub = (s: string) => s.split(root).join("<root>");

    async function add(typed: string, from: "rooted" | "neutral"): Promise<Resolution> {
      const cwd = from === "rooted" ? root : neutral;
      const settingsFile = path.join(root, `lua-${Math.random().toString(36).slice(2)}.ini`);
      fs.writeFileSync(settingsFile, "");
      const overrides = { scripts: path.join(root, "scripts") };

      const cli = new NscpInstance({ workDir: cwd, settingsFile, pathOverrides: overrides });
      const added = await cli.run(["lua", "add", "--script", typed], { allowFailure: true });

      const recorded = readSection(fs.readFileSync(settingsFile, "utf8"), "/settings/lua/scripts");

      // A second process, started somewhere else: this is the service's view.
      const agent = new NscpInstance({ workDir: neutral, settingsFile, pathOverrides: overrides });
      const boot = await agent.run(
        ["client", "--module", "LUAScript", "--boot", "--log", "debug", "--query", "no_such"],
        { allowFailure: true },
      );

      return {
        typed,
        answered: answer(added.all ?? ""),
        recorded: recorded?.value,
        worksElsewhere: /Adding script:/.test(boot.all ?? ""),
      };
    }

    beforeAll(async () => {
      root = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-luacli-"));
      neutral = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-luacli-neutral-"));
      for (const rel of ["a.lua", "scripts/b.lua", "scripts/lua/c.lua", "scripts/lua/foo/d.lua"]) {
        const p = path.join(root, rel);
        fs.mkdirSync(path.dirname(p), { recursive: true });
        fs.writeFileSync(p, "-- probe\n");
      }
      available = (await add("c.lua", "rooted")).worksElsewhere;
    });

    it("prints what each spelling records and resolves to", async () => {
      if (!available) {
        console.warn("LUAScript not built in this configuration - skipped");
        return;
      }
      const rows: Resolution[] = [];
      for (const typed of ["c.lua", "lua/c.lua", "foo/d.lua", "b.lua", "scripts/lua/c.lua"]) {
        rows.push(await add(typed, "rooted"));
      }
      table("nscp lua add --script <typed>, run from the install directory", rows, scrub);

      // Recorded verbatim in every accepted case - the CLI does not normalise.
      for (const r of rows) expect(r.recorded).toBe(r.typed);

      // The four the loader can find on its own keep working anywhere.
      for (const r of rows.slice(0, 4)) expect(r.worksElsewhere).toBe(true);

      // ...and the trap: accepted here, because the CLI tried the value against
      // its own working directory first and the file was there. The service
      // runs elsewhere and never finds it.
      const trap = rows[rows.length - 1];
      expect(trap.typed).toBe("scripts/lua/c.lua");
      expect(trap.answered).toMatch(/^Added /);
      expect(trap.worksElsewhere).toBe(false);
    });

    it("refuses that same spelling when run from anywhere else", async () => {
      if (!available) return;
      const r = await add("scripts/lua/c.lua", "neutral");
      table("nscp lua add --script, run from a neutral directory", [r], scrub);
      expect(r.answered).toMatch(/Script not found/);
      expect(r.recorded).toBeUndefined();
    });

    it("prints its listing relative to ${scripts}, and add takes it straight back", async () => {
      if (!available) return;

      const settingsFile = path.join(root, "lua-list.ini");
      fs.writeFileSync(settingsFile, "");
      const overrides = { scripts: path.join(root, "scripts") };

      // Listed from a neutral directory on purpose: the listing must not depend
      // on where the CLI was run, and neither must feeding it back.
      const lister = new NscpInstance({ workDir: neutral, settingsFile, pathOverrides: overrides });
      const listed = await lister.run(["lua", "list"], { allowFailure: true });
      const entries = (listed.all ?? "")
        .split("\n")
        .map((l) => l.trim())
        .filter((l) => /\.lua$/.test(l))
        .map((l) => l.split("\\").join("/"));

      console.log(`\n  nscp lua list, run from a neutral directory\n  ${"-".repeat(42)}\n` + entries.map((e) => `  ${e}`).join("\n"));

      // ${scripts}-relative, not ${base-path}-relative and not absolute. The
      // ${base-path} spelling (`scripts/lua/c.lua`) is the trap the test above
      // pins: find_file has no candidate for it, so a listing printed that way
      // is one `add` refuses from anywhere but the install directory.
      expect(entries).toEqual(expect.arrayContaining(["lua/c.lua", "lua/foo/d.lua"]));
      for (const e of entries) {
        expect(path.isAbsolute(e)).toBe(false);
        expect(e.startsWith("scripts/")).toBe(false);
      }

      // The round trip: every value the listing printed is one `add` accepts
      // from elsewhere, and that the service then resolves.
      const rows: Resolution[] = [];
      for (const entry of entries) rows.push(await add(entry, "neutral"));
      table("nscp lua add --script <what list printed>, run from a neutral directory", rows, scrub);
      for (const r of rows) {
        expect(r.answered).toMatch(/^Added /);
        expect(r.recorded).toBe(r.typed);
        expect(r.worksElsewhere).toBe(true);
      }
    });

    it("records an absolute path outside the script folder as given", async () => {
      if (!available) return;
      const vendor = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-luavendor-"));
      const script = path.join(vendor, "v.lua");
      fs.writeFileSync(script, "-- probe\n");
      const r = await add(script, "neutral");
      table("nscp lua add --script <absolute, outside ${scripts}>", [r], (s) =>
        s.split(vendor).join("<vendor>"),
      );
      expect(r.recorded).toBe(script);
      expect(r.worksElsewhere).toBe(true);
    });
  });

  // --- external scripts -----------------------------------------------------
  describeOnLinux("nscp ext-scr add", () => {
    let root: string;
    let neutral: string;
    const scrub = (s: string) => s.split(root).join("<root>");

    async function add(typed: string, from: "rooted" | "neutral"): Promise<Resolution> {
      const cwd = from === "rooted" ? root : neutral;
      const settingsFile = path.join(root, `ext-${Math.random().toString(36).slice(2)}.ini`);
      fs.writeFileSync(settingsFile, "");
      const overrides = { scripts: path.join(root, "scripts") };

      const cli = new NscpInstance({ workDir: cwd, settingsFile, pathOverrides: overrides });
      const added = await cli.run(["ext-scr", "add", "--script", typed], { allowFailure: true });

      const recorded = readSection(
        fs.readFileSync(settingsFile, "utf8"),
        "/settings/external scripts/scripts",
      );

      let worksElsewhere = false;
      if (recorded) {
        const agent = new NscpInstance({
          workDir: neutral,
          settingsFile,
          pathOverrides: overrides,
        });
        const run = await agent.run(
          ["client", "--module", "CheckExternalScripts", "--boot", "--query", recorded.alias],
          { allowFailure: true },
        );
        worksElsewhere = /probe ok/.test(run.all ?? "");
      }
      return {
        typed,
        answered: answer(added.all ?? ""),
        recorded: recorded?.value,
        worksElsewhere,
      };
    }

    beforeAll(() => {
      root = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-extcli-"));
      neutral = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-extcli-neutral-"));
      const script = path.join(root, "scripts", "b.sh");
      fs.mkdirSync(path.dirname(script), { recursive: true });
      fs.writeFileSync(script, "#!/bin/sh\necho 'probe ok'\n");
      fs.chmodSync(script, 0o755);
    });

    it("prints what each spelling records and whether the command then runs", async () => {
      const rows: Resolution[] = [];
      for (const typed of [path.join(root, "scripts", "b.sh"), "scripts/b.sh", "b.sh"]) {
        rows.push(await add(typed, "rooted"));
      }
      table("nscp ext-scr add --script <typed>, run from the install directory", rows, scrub);

      // Absolute: recorded as given, and the command runs from anywhere. This
      // module resolves nothing at run time, so that is the only spelling that
      // is independent of how the agent was started.
      expect(rows[0].recorded).toBe(path.join(root, "scripts", "b.sh"));
      expect(rows[0].worksElsewhere).toBe(true);

      // Relative: accepted here and recorded verbatim, but the value is a
      // command line handed to the OS, so it only runs where the working
      // directory happens to contain `scripts`.
      expect(rows[1].recorded).toBe("scripts/b.sh");
      expect(rows[1].worksElsewhere).toBe(false);

      // A bare name is refused rather than recorded: it would become a PATH
      // lookup at run time, not a file in the script folder.
      expect(rows[2].answered).toMatch(/Script not found/);
      expect(rows[2].recorded).toBeUndefined();
    });
  });
});
