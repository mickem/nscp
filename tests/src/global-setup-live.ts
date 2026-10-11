/**
 * Global setup for the live/remote acceptance suite (jest.live.config.js).
 *
 * The normal global-setup.ts resolves NSCP_BIN and (optionally) probes docker,
 * because the standard suites spawn nscp locally. The live suite spawns
 * nothing — it talks to an already-running server — so those preconditions do
 * not apply. Here we only validate that a target has been configured, and fail
 * fast with an actionable message otherwise.
 */
import { liveConfig } from "./live-target";

export default async function globalSetupLive(): Promise<void> {
  // The estate suite (live/estate.test.ts) reads its targets from a manifest,
  // one machine and password each, so there is no single target to validate.
  if (process.env.NSCP_ESTATE_MANIFEST && !process.env.NSCP_TARGET_PASSWORD) {
    // eslint-disable-next-line no-console
    console.log(`[live] estate manifest ${process.env.NSCP_ESTATE_MANIFEST}`);
    return;
  }
  const cfg = liveConfig(); // throws with guidance if password is missing
  // eslint-disable-next-line no-console
  console.log(
    `[live] target ${cfg.base} as ${cfg.user}` +
      (cfg.insecure ? " (TLS verification disabled)" : "") +
      (cfg.os ? ` os=${cfg.os}` : ""),
  );
}
