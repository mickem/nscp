import globalSetup from "./global-setup";

/**
 * Global setup for the stress harness (jest.stress.config.js): the regular
 * one, which resolves NSCP_BIN, without the docker probe. Every stress scenario
 * is loopback inside one agent, so a daemon is never needed and its absence
 * must not abort the run.
 */
export default async function globalSetupStress(): Promise<void> {
  process.env.NSCP_SKIP_DOCKER = "1";
  await globalSetup();
}
