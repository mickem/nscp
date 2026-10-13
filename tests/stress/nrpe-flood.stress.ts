/**
 * Scenario 2: an NRPE flood, the path the retired test_stress.py routed its
 * load through.
 *
 * N workers hammer the agent's NRPEServer from a TypeScript NRPE v2 client
 * (stress/lib/nrpe-client.ts), one connection per request as check_nrpe does,
 * so a request costs the agent an accept, a packet and a dispatch and costs
 * the runner no process spawn. The mix is a trivial check that echoes a
 * per-request token (`check_ok message=<token>`, which proves the answer is
 * the request's own) and a collector-backed check (`check_cpu`, the 1 Hz CPU
 * ring under its lock).
 *
 * Twice over: in the clear, and over TLS against the server's cert mode with
 * mutual authentication (`verify mode = peer-cert`, certificates from the
 * shared TLS fixture), where every request also pays a handshake. Each mode
 * gets its own agent and its own report, nrpe-flood-plain.json and
 * nrpe-flood-tls.json. No web server: the post-load probe is an NRPE request
 * too, so this is the NRPE server and the dispatch path and nothing else.
 *
 * Pass/fail is on the invariants in stress/lib/invariants.ts: every request
 * answered with its own token, the agent prompt once the load stops, no
 * growth from warm-up to end, a clean log.
 */
import * as tls from "tls";

import { NscpInstance, OK, bundledSecurityFile, generateCertChain } from "@fixtures/index";

import { ENGINE_LOG_NOISE, NRPE_PORT } from "@stress/engines";
import { expectCleanLog, expectNoGrowth, expectResponsive } from "@stress/invariants";
import { readKnobs, scenarioTimeoutMs, warmupMs } from "@stress/knobs";
import { runLoad } from "@stress/load";
import { NrpeClient } from "@stress/nrpe-client";
import { startSampler } from "@stress/process-stats";
import { writeReport } from "@stress/report";

const knobs = readKnobs();
jest.setTimeout(scenarioTimeoutMs(knobs));

/** The thresholds that keep check_cpu OK on any machine, so a failure is a real one. */
const CPU_ARGS = ["warning=usage > 101", "critical=usage > 101"];

/**
 * Log lines the set-up produces and the load never does: the port probes
 * (ENGINE_LOG_NOISE) and, in TLS mode, the same probe hanging up during the
 * handshake. A request the flood loses to a failed handshake is counted by
 * the client, not hidden here.
 */
const LOG_NOISE = [
  ...ENGINE_LOG_NOISE,
  /nrpe Failed to establish secure connection: stream truncated/,
];

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

type Mode = "plain" | "tls";

describe.each<Mode>(["plain", "tls"])("stress: NRPE flood, %s", (mode) => {
  let nscp: NscpInstance;
  let nrpe: NrpeClient;

  beforeAll(async () => {
    nscp = new NscpInstance();
    const server: Record<string, string> = {
      port: String(NRPE_PORT),
      "allow arguments": "true",
      // The thresholds carry `>`.
      "allow nasty characters": "true",
      // The flood's workers with room to spare: a pool the load itself
      // exhausts would measure the pool size, not the agent.
      "thread pool": String(knobs.concurrency + 4),
      "use ssl": String(mode === "tls"),
    };
    let tlsOptions: tls.ConnectionOptions | undefined;
    if (mode === "tls") {
      const certs = generateCertChain({
        outDir: nscp.scratch("certs"),
        signed: {
          server: { commonName: "localhost", isServer: true },
          client: { commonName: "stress-client" },
        },
      });
      Object.assign(server, {
        certificate: certs.signed.server.certPath,
        "certificate key": certs.signed.server.keyPath,
        // certificate-path is a per-instance scratch directory with no DH
        // parameters in it; use the ones shipped with the build.
        dh: bundledSecurityFile("nrpe_dh_2048.pem"),
        ca: certs.ca.certPath,
        // Mutual TLS: the server wants the client's certificate and verifies
        // it against the CA that issued its own.
        "verify mode": "peer-cert",
      });
      tlsOptions = {
        ca: certs.ca.certPem,
        cert: certs.signed.client.certPem,
        key: certs.signed.client.keyPem,
        servername: "localhost",
        rejectUnauthorized: true,
      };
    }
    await nscp.configure({
      "/modules": { CheckSystem: "enabled", CheckHelpers: "enabled", NRPEServer: "enabled" },
      "/settings/NRPE/server": server,
    });
    await nscp.waitForPortFree(NRPE_PORT, { timeoutMs: 30_000 });
    nscp.start();
    await nscp.waitForPort(NRPE_PORT, { timeoutMs: 30_000 });
    nrpe = new NrpeClient(NRPE_PORT, "127.0.0.1", 120_000, tlsOptions);

    // check_cpu answers UNKNOWN until the collector has pushed its first sample.
    const deadline = performance.now() + 60_000;
    for (;;) {
      const a = await nrpe.query("check_cpu", CPU_ARGS);
      if (a.result === OK) break;
      if (performance.now() >= deadline) {
        throw new Error(`CPU collector produced no sample before the run: ${a.message}`);
      }
      await sleep(500);
    }
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("answers every request with its own token, stays prompt and does not grow", async () => {
    const pid = nscp.pid;
    if (pid === undefined) throw new Error("the agent is not running");
    const sampler = startSampler(pid);
    const mix = { check_ok: 0, check_cpu: 0 };

    // The mix, spread deterministically over each worker's own counter:
    // seven in ten the echo, three in ten the collector-backed check.
    const task = async (worker: number, n: number): Promise<void> => {
      if (n % 10 < 7) {
        mix.check_ok++;
        const token = `w${worker}-${n}`;
        await nrpe.expect("check_ok", [`message=${token}`], token);
      } else {
        mix.check_cpu++;
        const a = await nrpe.query("check_cpu", CPU_ARGS);
        if (a.result !== OK) throw new Error(`check_cpu answered ${a.result}: ${a.message}`);
      }
    };

    // Warm up, then take the RSS baseline the leak canary measures from.
    const warmup = await runLoad({
      durationMs: warmupMs(knobs),
      concurrency: knobs.concurrency,
      rps: knobs.rps,
      task,
    });
    await sampler.markBaseline();
    const counted = { ...mix };
    const retriesBefore = nrpe.localRetries;

    const load = await runLoad({
      durationMs: knobs.durationSeconds * 1000,
      concurrency: knobs.concurrency,
      rps: knobs.rps,
      task,
    });

    await sampler.markEnd();

    // Post-load: the agent answers a fresh check promptly.
    const responsiveAfterMs = await expectResponsive(async () => {
      const a = await nrpe.query("check_ok");
      if (a.result !== OK) throw new Error(`check_ok answered ${a.result}: ${a.message}`);
    });
    const samples = await sampler.stop();

    const growth = expectNoGrowth(sampler, knobs.rssTolerancePct);
    writeReport({
      scenario: `nrpe-flood-${mode}`,
      startedAt: new Date(Date.now() - samples[samples.length - 1].t * 1000).toISOString(),
      platform: process.platform,
      knobs,
      load,
      responsiveAfterMs,
      growth,
      samples,
      extra: {
        mode,
        warmup,
        mix: {
          check_ok: mix.check_ok - counted.check_ok,
          check_cpu: mix.check_cpu - counted.check_cpu,
        },
        // Requests that waited for a local ephemeral port: time the agent
        // never saw, and a sign the runner, not the agent, is the limit.
        localPortRetries: nrpe.localRetries - retriesBefore,
      },
    });

    // Zero failed requests, in the warm-up as well as the run.
    expect(warmup.errors).toEqual({ count: 0, samples: [] });
    expect(load.errors).toEqual({ count: 0, samples: [] });
    expect(load.requests).toBeGreaterThan(0);
    expectCleanLog(nscp, LOG_NOISE);
  });
});
