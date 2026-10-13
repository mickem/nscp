/**
 * Global setup for `npm run test:estate` (jest.estate.config.js): find the
 * estate to test and hand it to live/estate.test.ts through
 * NSCP_ESTATE_MANIFEST.
 *
 * With NSCP_ESTATE_MANIFEST already set, that is the estate. Otherwise every
 * manifest under build/powershell that still lists a machine that is up (see
 * estate-manifest.ts), so a suite kept with `run-estate.ps1 <role> -KeepVms`
 * is tested with no path to type. Nothing up fails the run here instead of
 * passing an empty one.
 */
import * as path from "path";
import { liveMachines, resolveManifests } from "./estate-manifest";

export default async function globalSetupEstate(): Promise<void> {
  const manifests = resolveManifests();
  const machines = manifests.flatMap((m) =>
    liveMachines(m).map((e) => `${e.Name} (${e.Role || "base"})`),
  );
  if (machines.length === 0) {
    throw new Error(
      process.env.NSCP_ESTATE_MANIFEST
        ? `[estate] no machine that is up in ${process.env.NSCP_ESTATE_MANIFEST}`
        : "[estate] no estate is up: no manifest under build/powershell lists a machine whose " +
            ".vm.<name>.pwd still exists. Keep one with ./build/powershell/run-estate.ps1 <role> -KeepVms, " +
            "or point NSCP_ESTATE_MANIFEST at a manifest.",
    );
  }
  // Read by the test file: its environment is created after this runs.
  process.env.NSCP_ESTATE_MANIFEST = manifests.join(path.delimiter);
  // eslint-disable-next-line no-console
  console.log(`[estate] ${machines.length} machine(s): ${machines.join(", ")}`);
  // eslint-disable-next-line no-console
  for (const m of manifests) console.log(`[estate]   ${m}`);
}
