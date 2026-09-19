/**
 * Which folders a configured script is looked for in, and which it may be
 * loaded from.
 *
 * Three modules resolve scripts and none of them do it the same way, so this
 * pins the actual behaviour of each against a tree laid out like a real
 * install:
 *
 *   <root>/a.py                       above the script folder
 *   <root>/scripts/b.py               ${scripts}
 *   <root>/scripts/python/c.py        the module's own folder
 *   <root>/scripts/python/foo/d.py    nested below it
 *
 * The interesting part is not which spellings work but that the answer depends
 * on the process working directory: the search tries the configured value
 * as-is first, so the same nsclient.ini resolves differently depending on how
 * the agent was started. Both cases are covered - `rooted` runs with the
 * install root as the working directory (a Windows service, or the shipped
 * systemd unit), `neutral` runs from somewhere else entirely.
 *
 * Outcomes are read from the log rather than from a check result, because
 * "not found" and "found but refused" are different answers and a check result
 * cannot tell them apart.
 */
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { NscpInstance } from "@fixtures/index";

jest.setTimeout(180_000);

type Outcome = "loaded" | "not-found" | "refused";

/** Lay out the probe tree under `root` for the given module's extension. */
function buildTree(root: string, sub: string, ext: string): void {
  const write = (p: string) => {
    fs.mkdirSync(path.dirname(p), { recursive: true });
    fs.writeFileSync(p, ext === ".lua" ? "-- probe\n" : "# probe\n");
  };
  write(path.join(root, `a${ext}`));
  write(path.join(root, "scripts", `b${ext}`));
  write(path.join(root, "scripts", sub, `c${ext}`));
  write(path.join(root, "scripts", sub, "foo", `d${ext}`));
}

function classify(output: string): Outcome {
  if (/Refusing to load script outside/.test(output)) return "refused";
  if (/Adding script:/.test(output)) return "loaded";
  return "not-found";
}

describe("script folder resolution", () => {
  /**
   * One probe: configure `value` as the module's only script and report what
   * happened. `cwd` selects whether the agent runs from the install root.
   */
  async function probe(opts: {
    module: string;
    section: string;
    root: string;
    value: string;
    cwd: "rooted" | "neutral";
    extraRoots?: string;
  }): Promise<Outcome> {
    // workDir is the process working directory, so pointing it at the install
    // root is what makes the CWD-first candidate fire.
    const workDir =
      opts.cwd === "rooted" ? opts.root : fs.mkdtempSync(path.join(os.tmpdir(), "nscp-neutral-"));
    const nscp = new NscpInstance({
      workDir,
      settingsFile: path.join(workDir, `probe-${Math.random().toString(36).slice(2)}.ini`),
      pathOverrides: { scripts: path.join(opts.root, "scripts") },
    });
    fs.writeFileSync(
      nscp.settingsFile,
      [
        "[/modules]",
        `${opts.module} = enabled`,
        "",
        ...(opts.extraRoots
          ? [`[/settings/${opts.section}]`, `additional script roots = ${opts.extraRoots}`, ""]
          : []),
        `[/settings/${opts.section}/scripts]`,
        `probe = ${opts.value}`,
        "",
      ].join("\n"),
    );
    const r = await nscp.run(
      ["client", "--module", opts.module, "--boot", "--log", "debug", "--query", "no_such_command"],
      {
        allowFailure: true,
      },
    );
    return classify(r.all ?? `${r.stdout}\n${r.stderr}`);
  }

  // --- PythonScript / LUAScript share a shape, so drive them from one table --
  //
  // `python/c.py` and `foo/d.py` both work because the search tries the value
  // under the module's folder *and* directly under ${scripts}; the spellings
  // that include a literal "scripts" segment only work when the working
  // directory happens to be the install root, which is the point of the two
  // cwd columns.
  const matrix: Array<{
    value: (s: string, e: string) => string;
    rooted: Outcome;
    neutral: Outcome;
    why: string;
  }> = [
    {
      value: (_s, e) => `a${e}`,
      rooted: "refused",
      neutral: "not-found",
      why: "above the script folder",
    },
    {
      value: (_s, e) => `../a${e}`,
      rooted: "refused",
      neutral: "refused",
      why: "climbs out of the script folder",
    },
    {
      value: (_s, e) => `b${e}`,
      rooted: "loaded",
      neutral: "loaded",
      why: "directly in ${scripts}",
    },
    {
      value: (_s, e) => `scripts/b${e}`,
      rooted: "loaded",
      neutral: "not-found",
      why: "only via the working directory",
    },
    {
      value: (_s, e) => `c${e}`,
      rooted: "loaded",
      neutral: "loaded",
      why: "in the module's folder",
    },
    {
      value: (s, e) => `${s}/c${e}`,
      rooted: "loaded",
      neutral: "loaded",
      why: "explicit module folder",
    },
    {
      value: (s, e) => `scripts/${s}/c${e}`,
      rooted: "loaded",
      neutral: "not-found",
      why: "only via the working directory",
    },
    {
      value: (_s, e) => `d${e}`,
      rooted: "not-found",
      neutral: "not-found",
      why: "nested, bare name does not reach it",
    },
    {
      value: (_s, e) => `foo/d${e}`,
      rooted: "loaded",
      neutral: "loaded",
      why: "nested below the module's folder",
    },
    {
      value: (s, e) => `${s}/foo/d${e}`,
      rooted: "loaded",
      neutral: "loaded",
      why: "explicit nested path",
    },
  ];

  describe.each([
    { module: "PythonScript", section: "python", sub: "python", ext: ".py" },
    { module: "LUAScript", section: "lua", sub: "lua", ext: ".lua" },
  ])("$module", ({ module, section, sub, ext }) => {
    let root: string;
    let available = false;

    beforeAll(async () => {
      root = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-scriptroot-"));
      buildTree(root, sub, ext);
      // The module is optional at build time (Python needs Boost.Python, Lua
      // needs liblua). Probe once: a build without it should skip, not fail.
      available =
        (await probe({ module, section, root, value: `c${ext}`, cwd: "neutral" })) === "loaded";
    });

    it("is built in this configuration", () => {
      if (!available) console.warn(`${module} not available in this build - folder matrix skipped`);
      expect(true).toBe(true);
    });

    for (const row of matrix) {
      for (const cwd of ["rooted", "neutral"] as const) {
        const value = row.value(sub, ext);
        const expected = cwd === "rooted" ? row.rooted : row.neutral;
        it(`${cwd}: '${value}' is ${expected} (${row.why})`, async () => {
          if (!available) return;
          expect(await probe({ module, section, root, value, cwd })).toBe(expected);
        });
      }
    }

    // --- the sandbox, and the escape hatch for scripts we do not own --------

    it("refuses an absolute path outside the script folder", async () => {
      if (!available) return;
      const vendor = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-vendor-"));
      fs.writeFileSync(path.join(vendor, `v${ext}`), ext === ".lua" ? "-- v\n" : "# v\n");
      expect(
        await probe({ module, section, root, value: path.join(vendor, `v${ext}`), cwd: "neutral" }),
      ).toBe("refused");
    });

    it("loads that same script once its folder is an additional root", async () => {
      if (!available) return;
      const vendor = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-vendor-"));
      fs.writeFileSync(path.join(vendor, `v${ext}`), ext === ".lua" ? "-- v\n" : "# v\n");
      expect(
        await probe({
          module,
          section,
          root,
          value: path.join(vendor, `v${ext}`),
          cwd: "neutral",
          extraRoots: vendor,
        }),
      ).toBe("loaded");
    });

    it("expands path tokens in the additional roots", async () => {
      if (!available) return;
      // ${scripts}/.. is the install root, which is where a.py sits.
      expect(
        await probe({
          module,
          section,
          root,
          value: `../a${ext}`,
          cwd: "neutral",
          extraRoots: "${scripts}/..",
        }),
      ).toBe("loaded");
    });

    it("keeps refusing when the additional roots name somewhere else", async () => {
      if (!available) return;
      expect(
        await probe({
          module,
          section,
          root,
          value: `../a${ext}`,
          cwd: "neutral",
          extraRoots: "/nonexistent-root",
        }),
      ).toBe("refused");
    });
  });

  // --- CheckExternalScripts resolves nothing itself --------------------------
  //
  // Its value is a command line handed to the operating system, not a path the
  // agent resolves. So there is no search, no sandbox, and - the part that
  // catches people out - no ${...} expansion. Pinned here because it is the
  // opposite of the two modules above and the docs now promise it.
  (process.platform === "linux" ? describe : describe.skip)("CheckExternalScripts", () => {
    let root: string;

    /** Configure one command and return its output. */
    async function runScript(value: string, cwd: "rooted" | "neutral"): Promise<string> {
      const workDir =
        cwd === "rooted" ? root : fs.mkdtempSync(path.join(os.tmpdir(), "nscp-ext-neutral-"));
      const nscp = new NscpInstance({
        workDir,
        settingsFile: path.join(workDir, `ext-${Math.random().toString(36).slice(2)}.ini`),
        pathOverrides: { scripts: path.join(root, "scripts") },
      });
      fs.writeFileSync(
        nscp.settingsFile,
        [
          "[/modules]",
          "CheckExternalScripts = enabled",
          "",
          "[/settings/external scripts/scripts]",
          `probe = ${value}`,
          "",
        ].join("\n"),
      );
      const r = await nscp.run(
        ["client", "--module", "CheckExternalScripts", "--boot", "--query", "probe"],
        { allowFailure: true },
      );
      return r.all ?? `${r.stdout}\n${r.stderr}`;
    }

    beforeAll(() => {
      root = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-extroot-"));
      const script = path.join(root, "scripts", "b.sh");
      fs.mkdirSync(path.dirname(script), { recursive: true });
      fs.writeFileSync(script, "#!/bin/sh\necho 'probe ok'\nexit 0\n");
      fs.chmodSync(script, 0o755);
    });

    it("runs a relative command against the working directory", async () => {
      expect(await runScript("scripts/b.sh", "rooted")).toMatch(/probe ok/);
    });

    it("cannot find that same command from another working directory", async () => {
      expect(await runScript("scripts/b.sh", "neutral")).not.toMatch(/probe ok/);
    });

    it("does not expand path tokens", async () => {
      // The documented trap: ${scripts}/b.sh reaches the shell literally.
      expect(await runScript("${scripts}/b.sh", "rooted")).not.toMatch(/probe ok/);
    });

    it("treats a name with no separator as a PATH lookup, not a local file", async () => {
      // Even standing in the folder that holds it - which is why a bare script
      // name never works here, unlike the Python and Lua sections above.
      const workDir = path.join(root, "scripts");
      const nscp = new NscpInstance({
        workDir,
        settingsFile: path.join(workDir, "bare.ini"),
        pathOverrides: { scripts: path.join(root, "scripts") },
      });
      fs.writeFileSync(
        nscp.settingsFile,
        [
          "[/modules]",
          "CheckExternalScripts = enabled",
          "",
          "[/settings/external scripts/scripts]",
          "probe = b.sh",
          "",
        ].join("\n"),
      );
      const r = await nscp.run(
        ["client", "--module", "CheckExternalScripts", "--boot", "--query", "probe"],
        { allowFailure: true },
      );
      expect(r.all ?? "").not.toMatch(/probe ok/);
    });

    it("runs an absolute command from anywhere", async () => {
      expect(await runScript(path.join(root, "scripts", "b.sh"), "neutral")).toMatch(/probe ok/);
    });
  });
});
