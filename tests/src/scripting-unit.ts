import * as fs from "fs";
import * as os from "os";
import * as path from "path";

import { NscpInstance, NscpRunResult } from "./nscp";

/** The repository's own `scripts/` folder, which `tests/` sits next to. */
export const REPO_SCRIPTS = path.resolve(__dirname, "..", "..", "scripts");

/**
 * An NscpInstance whose `${scripts}` is a scratch copy of the repository's
 * `scripts/<lang>` folder, `lib/` included.
 *
 * The in-process test scripts are not packaged, so a CI job used to copy the
 * whole folder over the install before running them. Copying it here instead
 * runs the scripts from this checkout against whatever nscp is under test, and
 * leaves the install alone. It also means `${scripts}` is not next to the
 * binary, which is exactly where LUAScript's `require()` used to fail to look.
 */
export function scriptingUnitInstance(lang: "python" | "lua"): NscpInstance {
  const workDir = fs.mkdtempSync(path.join(os.tmpdir(), `nscp-unit-${lang}-`));
  const scriptsDir = path.join(workDir, "scripts");
  fs.cpSync(path.join(REPO_SCRIPTS, lang), path.join(scriptsDir, lang), { recursive: true });
  return new NscpInstance({ workDir, pathOverrides: { scripts: scriptsDir } });
}

export interface ScriptingUnitOptions {
  /** `--case <text>`, one per entry: run only the suites whose title contains it. */
  cases?: string[];
  /** `--show-all`: log passing results too. */
  showAll?: boolean;
  /** Timeout for the whole run, default 120 s. */
  timeout?: number;
}

/**
 * `nscp unit --language <lang> --script <script>`: boot the scripting module,
 * let the script write its own configuration, reload, and query
 * `<py|lua>_unittest`. Never throws on a non-zero exit; the caller asserts.
 */
export async function runScriptingUnit(
  nscp: NscpInstance,
  lang: "python" | "lua",
  script: string,
  opts: ScriptingUnitOptions = {},
): Promise<NscpRunResult> {
  const args = ["unit", "--language", lang, "--script", script];
  for (const c of opts.cases ?? []) args.push("--case", c);
  if (opts.showAll) args.push("--show-all");
  return nscp.run(args, { allowFailure: true, timeout: opts.timeout ?? 120_000 });
}
