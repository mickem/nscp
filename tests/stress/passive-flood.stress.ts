/**
 * Scenario 4: a passive flood, NSCA submissions at rate into the agent's own
 * listener, every one of them accounted for.
 *
 * N workers submit from a TypeScript NSCA client (stress/lib/nsca-client.ts,
 * its connections made by the Python socket pump - see socket-pump.ts) to
 * NSCAServer, one connection per submission as send_nsca does. The sink is
 * the web server's passive result cache: the server posts every packet it
 * decodes on `CHANNEL`, WEBServer caches it under the service name, and the
 * test drains the cache ten times a second over REST. Every submission
 * carries a service name of its own and a message built from the same worker
 * and sequence numbers, so a result is only counted when it comes back under
 * the right key with the right status and the right text: a packet lost on
 * the way, dropped by the server, or crossed with a concurrent one all show up
 * as a lost or a mismatched submission.
 *
 * NSCA is the one passive protocol the agent receives. NRDP and NSCA-ng have
 * clients here but no server, so a flood of those would be a flood of
 * something else's receiver; they are out of scope.
 *
 * The cipher is a knob (NSCP_STRESS_NSCA_CIPHER, default aes256, the server's
 * own default): none, xor, aes128, aes192 or aes256. The other ciphers are
 * proven one by one in nsca-loopback.test.ts; a flood wants the one real
 * deployments use.
 *
 * Pass/fail is on the invariants in stress/lib/invariants.ts plus the
 * accounting above: nothing lost, nothing mismatched, the agent prompt once
 * the load stops, no growth from warm-up to end, a clean log.
 */
import { NscpInstance, OK, setupQueryNscp } from "@fixtures/index";

import { expectCleanLog, expectNoGrowth, expectResponsive } from "@stress/invariants";
import { readKnobs, scenarioTimeoutMs, warmupMs } from "@stress/knobs";
import { runLoad } from "@stress/load";
import { NSCA_CIPHERS, NscaClient, isNscaCipher } from "@stress/nsca-client";
import { startSampler } from "@stress/process-stats";
import { writeReport } from "@stress/report";
import { RestClient } from "@stress/rest-client";

const knobs = readKnobs();
jest.setTimeout(scenarioTimeoutMs(knobs));

/** NSCA listener for this scenario, off the 5667 default and the regular suites' ports. */
const NSCA_PORT = Number(process.env.NSCP_STRESS_NSCA_PORT ?? 15667);
const CIPHER = process.env.NSCP_STRESS_NSCA_CIPHER ?? "aes256";
if (!isNscaCipher(CIPHER)) {
  throw new Error(`NSCP_STRESS_NSCA_CIPHER=${CIPHER} is not one of ${NSCA_CIPHERS.join(", ")}`);
}
const PASSWORD = "stress-nsca-key";
const PAYLOAD_LENGTH = 512;
/** The channel NSCAServer posts to and WEBServer caches from. */
const CHANNEL = "STRESS_PASSIVE";
/** What the client claims to be; the server passes it through as the result's host. */
const SENDER = "stress-sender";
/**
 * Ten times a second, not twice: every poll's reply is built in the agent's
 * heap, and at a few thousand submissions a second a half-second drain is a
 * few thousand results in one response. The heap then swung 8 MiB with every
 * poll and its high-water mark took 40 s to settle, so the leak canary read
 * +22% from an agent that held 50 MiB flat for five minutes. A tenth of a
 * second keeps the swing at 3 MiB, about the agent rather than the poller.
 */
const POLL_MS = 100;
/**
 * The cache keeps `max entries` keyed results and evicts the oldest beyond
 * that: 1000 by default, which a flood of unique keys at a few thousand a
 * second drained twice a second would overflow between two polls - the
 * first run lost exactly 1000 per poll to it. Sized for the whole run, so
 * the only way a submission goes missing is the agent dropping it.
 */
const MAX_ENTRIES = 1_000_000;
/** How long after the load the last submissions get to land before they count as lost. */
const GRACE_MS = 15_000;

/** The service name and the message of submission `n` from `worker`. */
const serviceOf = (worker: number, n: number) => `w${worker}-${n}`;
const messageOf = (worker: number, n: number) => `${worker}/${n} via ${CIPHER}`;
const statusOf = (n: number) => n % 4;

interface CachedResult {
  key: string;
  command: string;
  host: string;
  status: number;
  message: string;
  count: number;
}

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

describe("stress: passive flood, NSCA into the result cache", () => {
  let nscp: NscpInstance;
  let client: RestClient;
  let nsca: NscaClient;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.waitForPortFree(NSCA_PORT, { timeoutMs: 30_000 });
    // The extra tree replaces the fixture's /modules entry whole, so name all three.
    await setupQueryNscp(nscp, "CheckHelpers", {
      "/modules": { CheckHelpers: "enabled", WEBServer: "enabled", NSCAServer: "enabled" },
      "/settings/NSCA/server": {
        port: String(NSCA_PORT),
        inbox: CHANNEL,
        encryption: CIPHER,
        password: PASSWORD,
        "payload length": String(PAYLOAD_LENGTH),
        // The flood's workers with room to spare: a pool the load itself
        // exhausts would measure the pool size, not the agent.
        "thread pool": String(knobs.concurrency + 4),
      },
      "/settings/WEB/server/results": {
        enabled: "true",
        channel: CHANNEL,
        // The service name, which is unique per submission, so a drained
        // poll reports each submission once.
        "primary index": "${command}",
        "max entries": String(MAX_ENTRIES),
      },
    });
    await nscp.waitForPort(NSCA_PORT, { timeoutMs: 30_000 });
    client = new RestClient(2);
    await client.login();
    nsca = new NscaClient({ port: NSCA_PORT, cipher: CIPHER, password: PASSWORD });
  });

  afterAll(async () => {
    nsca?.close();
    await client?.close();
    await nscp?.stop();
  });

  it("lands every submission, stays prompt and does not grow", async () => {
    const pid = nscp.pid;
    if (pid === undefined) throw new Error("the agent is not running");
    const sampler = startSampler(pid);

    // Everything sent and everything drained, over the warm-up and the run
    // both: a warm-up submission that never lands is as lost as any other.
    let sent = 0;
    const sentByStatus = [0, 0, 0, 0];
    let delivered = 0;
    const deliveredByStatus = [0, 0, 0, 0];
    let mismatched = 0;
    const mismatchSamples: string[] = [];
    const pollLatencies: number[] = [];

    /** Drain the cache once and account for every entry. */
    const drain = async (): Promise<number> => {
      const t0 = performance.now();
      const results = await client.getJson<CachedResult[]>("/api/v2/results");
      pollLatencies.push(performance.now() - t0);
      let n = 0;
      for (const r of results) {
        n += r.count;
        delivered += r.count;
        const m = /^w(\d+)-(\d+)$/.exec(r.command);
        const [worker, seq] = m ? [Number(m[1]), Number(m[2])] : [NaN, NaN];
        const expected = m
          ? { status: statusOf(seq), message: messageOf(worker, seq), host: SENDER }
          : undefined;
        if (
          !expected ||
          r.status !== expected.status ||
          r.message !== expected.message ||
          r.host !== expected.host ||
          r.count !== 1
        ) {
          mismatched += r.count;
          if (mismatchSamples.length < 10) {
            mismatchSamples.push(
              `${r.command}: status ${r.status} host ${JSON.stringify(r.host)} ` +
                `message ${JSON.stringify(r.message)} count ${r.count}` +
                (expected
                  ? `, expected ${expected.status} ${JSON.stringify(expected.message)}`
                  : ""),
            );
          }
        } else {
          deliveredByStatus[r.status] += r.count;
        }
      }
      return n;
    };

    // The drain runs beside the load, every POLL_MS, until told to stop.
    let draining = true;
    const drainLoop = (async () => {
      while (draining) {
        await sleep(POLL_MS);
        await drain();
      }
    })();

    const task = async (worker: number, n: number): Promise<void> => {
      await nsca.submit({
        host: SENDER,
        service: serviceOf(worker, n),
        status: statusOf(n),
        message: messageOf(worker, n),
      });
      // Counted once it is out: a submission that failed locally is the
      // load's error, not a lost one.
      sent++;
      sentByStatus[statusOf(n)]++;
    };

    // Warm up, then take the RSS baseline the leak canary measures from.
    const warmup = await runLoad({
      durationMs: warmupMs(knobs),
      concurrency: knobs.concurrency,
      rps: knobs.rps,
      task,
    });
    await sampler.markBaseline();
    const retriesBefore = nsca.localRetries;

    const load = await runLoad({
      durationMs: knobs.durationSeconds * 1000,
      concurrency: knobs.concurrency,
      rps: knobs.rps,
      task,
    });

    await sampler.markEnd();

    // The last submissions are still in the server's pool and the cache: give
    // them a moment to land before anything is called lost.
    const graceDeadline = performance.now() + GRACE_MS;
    while (delivered < sent && performance.now() < graceDeadline) await sleep(POLL_MS);
    draining = false;
    await drainLoop;
    await drain();

    // Post-load: the agent answers a fresh check promptly.
    const responsiveAfterMs = await expectResponsive(() => client.query("check_ok"));
    const samples = await sampler.stop();

    const growth = expectNoGrowth(sampler, knobs.rssTolerancePct);
    const sortedPolls = [...pollLatencies].sort((a, b) => a - b);
    writeReport({
      scenario: "passive-flood",
      startedAt: new Date(Date.now() - samples[samples.length - 1].t * 1000).toISOString(),
      platform: process.platform,
      knobs,
      load,
      responsiveAfterMs,
      growth,
      samples,
      extra: {
        cipher: CIPHER,
        payloadLength: PAYLOAD_LENGTH,
        warmup,
        sent,
        sentByStatus,
        delivered,
        deliveredByStatus,
        lost: sent - delivered,
        mismatched,
        mismatchSamples,
        pollLatencyMs: {
          p50: sortedPolls[Math.floor(sortedPolls.length / 2)] ?? 0,
          max: sortedPolls[sortedPolls.length - 1] ?? 0,
        },
        // Submissions that waited for a local ephemeral port: time the agent
        // never saw, and a sign the runner, not the agent, is the limit.
        localPortRetries: nsca.localRetries - retriesBefore,
      },
    });

    // Zero failed submissions, in the warm-up as well as the run.
    expect(warmup.errors).toEqual({ count: 0, samples: [] });
    expect(load.errors).toEqual({ count: 0, samples: [] });
    expect(load.requests).toBeGreaterThan(0);
    // Zero lost and zero mismatched: every submission came back once, under
    // its own key, with its own status, host and message.
    expect({ sent, delivered, lost: sent - delivered }).toEqual({ sent, delivered: sent, lost: 0 });
    expect({ mismatched, samples: mismatchSamples }).toEqual({ mismatched: 0, samples: [] });
    expect(deliveredByStatus).toEqual(sentByStatus);
    expectCleanLog(nscp, [
      // The port probe that waits for the listener connects and hangs up
      // without a packet; the load never does that.
      /nsca Failed to read data: End of file/,
    ]);
  });
});
