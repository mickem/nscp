import globalSetup from "./global-setup";

/**
 * Global setup for the stress harness (jest.stress.config.js): the regular
 * one, which resolves NSCP_BIN, without the docker probe, and with the
 * floods' sockets in the Python pump. Every stress scenario is loopback
 * inside one agent, so a daemon is never needed and its absence must not
 * abort the run.
 */
export default async function globalSetupStress(): Promise<void> {
  process.env.NSCP_SKIP_DOCKER = "1";
  // The floods' NRPE and NSCA connections go through the Python socket pump:
  // node's own loopback connect crashes now and then on Windows (libuv
  // #5106; see stress/lib/socket-pump.ts). NSCP_STRESS_SOCKETS=node puts
  // them back on node, to compare.
  process.env.NSCP_STRESS_SOCKETS ??= "python";
  await globalSetup();
}
