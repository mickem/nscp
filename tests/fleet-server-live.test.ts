/**
 * The agent against a real fleet server.
 *
 * Every other fleet suite drives a fake server written in node. Those are where
 * edge cases belong — malformed responses, hostile payloads, a bundle signed by
 * the wrong key — because a fake can produce them on demand and in milliseconds.
 *
 * What a fake cannot do is disagree with the agent. When the server changed what
 * a bundle signature covers (from the bare SHA-256 digest to a descriptor of the
 * bundle's identity), every fake in this repo went on signing the old way, the
 * whole suite stayed green, and the first thing to notice was a live agent
 * reporting `signature verification failed` against a freshly enrolled host.
 *
 * This suite exists to make that a test failure instead. It runs the published
 * image on `latest` — floating deliberately — and walks the one path that
 * exercises the whole contract end to end:
 *
 *   provision a host -> enroll -> poll desired state -> verify a genuinely
 *   server-signed bundle -> apply it -> report in sync
 *
 * It is deliberately a single happy path. Anything it asserts beyond that is
 * covered more cheaply, and more thoroughly, by the fake-server suites.
 */
import * as fs from "fs";
import * as os from "os";
import * as path from "path";

import {
  NscpInstance,
  dockerOrSkip,
  startFleetServer,
  type StartedFleetServer,
} from "@fixtures/index";

// Pulling the image and standing up a CA is slow, and the on-prem tier lets an
// agent poll every 15s, so a first apply can legitimately take a while.
jest.setTimeout(300_000);

dockerOrSkip()("agent against a real fleet server", () => {
  let fleet: StartedFleetServer;
  let nscp: NscpInstance;
  let workDir: string;
  let hostId: string;

  /** Poll until `predicate` holds, failing with `what` (and the reason) on timeout. */
  async function waitFor(
    what: string,
    predicate: () => boolean | Promise<boolean>,
    timeoutMs = 120_000,
  ): Promise<void> {
    const started = Date.now();
    for (;;) {
      if (await predicate()) return;
      if (Date.now() - started > timeoutMs) {
        throw new Error(`Timed out after ${timeoutMs}ms waiting for ${what}`);
      }
      await new Promise((r) => setTimeout(r, 500));
    }
  }

  beforeAll(async () => {
    fleet = await startFleetServer();
    await fleet.api.login();

    workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-fleet-live-"));
    nscp = new NscpInstance({ workDir, pathOverrides: { "shared-path": workDir } });

    // Everything the host needs is set up before it ever polls, so the first
    // desired state it is handed already carries the bundle. Ordering it the
    // other way would work too, but would cost a poll interval per test run.
    const host = await fleet.api.createHost();
    hostId = host.hostId;
    // An operator-set tag, not an agent-reported one: the test then does not
    // depend on what this particular machine reports about itself.
    await fleet.api.setHostTag(hostId, "env", "integration");

    const bundle = await fleet.api.composeBundle("live-demo", "1.0.0", {
      settings: { "fleet live": { greeting: "hello-from-the-real-server" } },
    });
    const group = await fleet.api.createGroup("integration", {
      // No `source`: an omitted one means operator-set tags only, which is
      // exactly the tag above. Naming a source would have to be "manual",
      // "agent" or "any" — anything else is silently ignored, so spelling it
      // wrong would look like it worked.
      clauses: [{ op: "eq", key: "env", value: "integration" }],
    });
    await fleet.api.assignBundle(group.id, bundle.id, 100);

    const enrolled = await nscp.run(
      [
        "enroll",
        "--server",
        fleet.url,
        "--token",
        host.bootstrapToken,
        // The container generates its own certificate, so the CA that issued it
        // is not in any trust store. Both flags are needed: --insecure alone
        // does not turn verification off over https://.
        "--verify",
        "none",
        "--insecure",
      ],
      { allowFailure: true },
    );
    expect(enrolled.exitCode).toBe(0);
  });

  afterAll(async () => {
    if (nscp) await nscp.stop();
    if (fleet) await fleet.stop();
  });

  it("enrolls against the real server and stores the identity it was issued", () => {
    const manifest = path.join(workDir, "security", "agent-state.json");
    expect(fs.existsSync(manifest)).toBe(true);

    const state = JSON.parse(fs.readFileSync(manifest, "utf8"));
    // A real certificate from the tenant CA, not a fixture string.
    expect(state.cert_pem).toContain("BEGIN CERTIFICATE");
    expect(state.private_key_pem).toMatch(/BEGIN (RSA )?PRIVATE KEY/);
    // The Ed25519 key this suite is really about: bundles are verified against
    // it, and it has to be the key the server signs with.
    expect(state.bundle_signing_pub_pem).toContain("BEGIN PUBLIC KEY");
    expect(state.mtls_url).toContain("127.0.0.1");
  });

  it("verifies and applies a bundle the server actually signed", async () => {
    nscp.start();

    const fleetIni = path.join(workDir, "fleet", "fleet.ini");
    await waitFor("the composed bundle to be applied", () => {
      if (!fs.existsSync(fleetIni)) return false;
      return fs.readFileSync(fleetIni, "utf8").includes("hello-from-the-real-server");
    });

    const ini = fs.readFileSync(fleetIni, "utf8");
    expect(ini).toContain("[/settings/fleet live]");
    expect(ini).toMatch(/greeting\s*=\s*hello-from-the-real-server/);

    // Nothing may have been refused on the way. A verification failure is
    // reported rather than thrown, so the apply above could in principle have
    // succeeded on a later bundle while an earlier one was rejected.
    const log = nscp.capturedStdout() + nscp.capturedStderr();
    expect(log).not.toContain("failed verification");
    expect(log).not.toContain("signature verification failed");
  });

  it("reports itself in sync, and the server agrees", async () => {
    // The server's own view is the other half of the contract: the agent can
    // only be in_sync if the state hash it reported back matches what the
    // server computed and handed out.
    await waitFor("the server to consider the host in sync", async () => {
      const host = await fleet.api.getHost(hostId);
      return host?.status === "in_sync";
    });

    const host = await fleet.api.getHost(hostId);
    expect(host.status).toBe("in_sync");
    expect(host.enrolled_at).toBeTruthy();
    expect(host.current_state_hash).toBeTruthy();
  });
});
