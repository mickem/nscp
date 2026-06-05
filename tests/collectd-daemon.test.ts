/**
 * End-to-end check against a REAL collectd daemon (collectd-daemon, not our own
 * decoder): proves NSClient++'s CollectdClient speaks the binary network
 * protocol well enough for an actual collectd to parse, type-check, and store
 * the value-lists we send.
 *
 * Topology:
 *   nscp (host process) --UDP collectd binary--> collectd `network` plugin
 *                                                 --> collectd `csv` plugin
 *                                                 --> /data/<host>/<plugin>/<type>
 *
 * collectd's csv plugin only writes a file once it has accepted a value-list —
 * i.e. parsed the packet AND matched the value count/types against types.db —
 * so the presence of /data/it-collectd-host/cpu-total/cpu-user-* is positive
 * proof the wire format is correct, not just self-consistent with our decoder
 * (that is what collectd-submit.test.ts covers).
 *
 * collectd's UDP port is published with a raw `docker run -p ...:25826/udp`
 * because testcontainers only maps TCP; dockerd's proxy forwards the UDP
 * datagrams from loopback to the container. Everything is read back through
 * `docker exec`, so no UDP needs to flow host<-container.
 */
import * as path from "path";
import execa from "execa";
import { NscpInstance, dockerOrSkip } from "@fixtures/index";

jest.setTimeout(600_000);

const HOSTNAME = "it-collectd-host";
const IMAGE_TAG = "nscp_collectd_it";

/** Thin `docker ...` wrapper. */
async function docker(args: string[], opts: { reject?: boolean } = {}) {
  return execa("docker", args, { reject: opts.reject ?? true, all: true });
}

dockerOrSkip()("CollectD real-daemon integration", () => {
  let nscp: NscpInstance;
  let containerId = "";

  /** Files collectd's csv plugin has written for our host so far. */
  async function dataFiles(): Promise<string[]> {
    const r = await docker(
      ["exec", containerId, "sh", "-c", `find /data/${HOSTNAME} -type f 2>/dev/null || true`],
      { reject: false },
    );
    return r.stdout
      .split("\n")
      .map((s) => s.trim())
      .filter(Boolean);
  }

  /** Poll collectd's output dir until `predicate` holds (or the deadline passes). */
  async function waitForFiles(
    predicate: (files: string[]) => boolean,
    timeoutMs = 90_000,
  ): Promise<string[]> {
    const deadline = Date.now() + timeoutMs;
    let last: string[] = [];
    while (Date.now() < deadline) {
      last = await dataFiles();
      if (predicate(last)) return last;
      await new Promise((r) => setTimeout(r, 1000));
    }
    return last;
  }

  beforeAll(async () => {
    // Build directly with `docker build` (cwd = tests/, so tests/.dockerignore
    // keeps node_modules/ out of the context) and a fixed tag we can run
    // ourselves with a published UDP port.
    await execa("docker", ["build", "-t", IMAGE_TAG, "-f", "Dockerfiles/collectd.Dockerfile", "."], {
      cwd: path.resolve(__dirname),
      all: true,
    });

    const run = await execa(
      "docker",
      ["run", "-d", "-p", "127.0.0.1::25826/udp", IMAGE_TAG],
      { all: true },
    );
    containerId = run.stdout.trim();

    // Wait for "Initialization complete, entering read-loop." in the logs.
    const ready = await (async () => {
      const deadline = Date.now() + 30_000;
      while (Date.now() < deadline) {
        const logs = await docker(["logs", containerId], { reject: false });
        if (/entering read-loop/.test(`${logs.stdout}\n${logs.stderr}`)) return true;
        await new Promise((r) => setTimeout(r, 500));
      }
      return false;
    })();
    if (!ready) {
      const logs = await docker(["logs", containerId], { reject: false });
      throw new Error(`collectd did not become ready. Logs:\n${logs.stdout}\n${logs.stderr}`);
    }

    // The published loopback UDP port (docker assigns it; `docker port` reports it).
    const portOut = (await docker(["port", containerId, "25826/udp"])).stdout.trim();
    const port = Number(portOut.split("\n")[0].split(":").pop());
    if (!Number.isInteger(port) || port <= 0) {
      throw new Error(`could not parse mapped UDP port from: ${portOut}`);
    }

    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        CheckSystem: "enabled",
        CollectdClient: "enabled",
      },
      "/settings/core": {
        "metrics interval": "1s",
      },
      "/settings/collectd/client": {
        hostname: HOSTNAME,
      },
      "/settings/collectd/client/targets/default": {
        address: `127.0.0.1:${port}`,
      },
    });
    nscp.start();
  });

  afterAll(async () => {
    await nscp?.stop();
    if (containerId) await docker(["rm", "-f", containerId], { reject: false });
  });

  it("is parsed and stored by a real collectd daemon", async () => {
    // cpu (single DERIVE) and memory (single GAUGE) are standard collectd
    // types, so a real collectd accepts and stores them — but only if our
    // packet really is valid collectd binary protocol.
    const files = await waitForFiles(
      (f) => f.some((p) => /\/cpu-total\/cpu-/.test(p)) && f.some((p) => /\/memory\/memory-available/.test(p)),
    );
    expect(files.some((p) => /\/cpu-total\/cpu-user-/.test(p))).toBe(true);
    expect(files.some((p) => /\/memory\/memory-available-/.test(p))).toBe(true);

    // The stored CSV must contain collectd's "epoch,value" header plus at least
    // one data line — confirming collectd decoded a value, not just a header.
    const cpuFile = files.find((p) => /\/cpu-total\/cpu-user-/.test(p))!;
    const content = (await docker(["exec", containerId, "cat", cpuFile])).stdout;
    expect(content).toMatch(/^epoch,value/m);
    expect(content).toMatch(/^\d+(?:\.\d+)?,\S+$/m);
  });

  it("accepts the multi-value ps_count type", async () => {
    // ps_count is a two-value type (processes + threads) in types.db; collectd
    // drops value-lists whose value count doesn't match the type, so a written
    // ps_count file proves our multi-value encoding lines up with the spec.
    const files = await waitForFiles((f) => f.some((p) => /\/processes\/ps_count-/.test(p)));
    expect(files.some((p) => /\/processes\/ps_count-/.test(p))).toBe(true);
  });
});
