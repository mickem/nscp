/**
 * Finding the estate manifests for the role-aware live suite
 * (live/estate.test.ts).
 *
 * A manifest is written by build/powershell/provision-fleet-machines.ps1:
 * .fleet-machines.json for a hand-provisioned estate, and
 * .estate-runs/<ts>/suite-<role>/manifest.json for every run-estate.ps1
 * suite. Teardown drops a machine from its manifest and deletes its
 * .vm.<name>.pwd, so "the estate that is up" is every manifest entry that
 * provisioned cleanly and still has its pwd file.
 */
import * as fs from "fs";
import * as path from "path";

export interface ManifestEntry {
  Name: string;
  Role?: string;
  Family?: string;
  Os?: string;
  Status?: string;
  PwdFile?: string;
  PublicIp?: string;
}

export const POWERSHELL_DIR = path.resolve(__dirname, "..", "..", "build", "powershell");

/** PowerShell's ConvertTo-Json writes a one-element array as a bare object, and a BOM. */
export function readJson<T>(file: string): T {
  return JSON.parse(fs.readFileSync(file, "utf8").replace(/^﻿/, "")) as T;
}

/** The machines of one manifest that provisioned cleanly and are still up. */
export function liveMachines(manifest: string): ManifestEntry[] {
  if (!fs.existsSync(manifest)) return [];
  let raw: ManifestEntry | ManifestEntry[];
  try {
    raw = readJson<ManifestEntry | ManifestEntry[]>(manifest);
  } catch {
    return [];
  }
  return (Array.isArray(raw) ? raw : [raw]).filter(
    (m) => m && (!m.Status || m.Status === "OK") && !!m.PwdFile && fs.existsSync(m.PwdFile),
  );
}

/** Every manifest under build/powershell that still lists a machine that is up. */
export function discoverManifests(dir: string = POWERSHELL_DIR): string[] {
  const found = [path.join(dir, ".fleet-machines.json")];
  const runs = path.join(dir, ".estate-runs");
  if (fs.existsSync(runs)) {
    for (const run of fs.readdirSync(runs)) {
      const runDir = path.join(runs, run);
      if (!fs.statSync(runDir).isDirectory()) continue;
      for (const suite of fs.readdirSync(runDir)) {
        if (suite.startsWith("suite-")) found.push(path.join(runDir, suite, "manifest.json"));
      }
    }
  }
  return found.filter((m) => liveMachines(m).length > 0);
}

/**
 * NSCP_ESTATE_MANIFEST (one path, or several joined with the platform's path
 * delimiter) when set, else whatever discoverManifests() finds.
 */
export function resolveManifests(): string[] {
  const env = process.env.NSCP_ESTATE_MANIFEST;
  if (env) {
    return env
      .split(path.delimiter)
      .filter((p) => p.trim())
      .map((p) => path.resolve(p.trim()));
  }
  return discoverManifests();
}
