/**
 * Stands up a minimal collectd binary-protocol receiver and points
 * NSClient++'s CollectdClient at it, then verifies the metrics flow:
 *
 *   CheckSystem produces system.cpu.* / system.mem.* metrics; the core
 *   metrics scheduler fetches them every `metrics interval` and hands them to
 *   every module that consumes metrics. CollectdClient's metrics handler maps
 *   them onto collectd value-lists (cpu / memory / processes / …) and sends
 *   them to the configured target as collectd's binary network protocol over
 *   UDP (default port 25826).
 *
 * Unlike the graphite/nsca scenarios, the "server" here is NOT a docker
 * container: testcontainers (v10) only publishes TCP ports, and collectd's
 * wire protocol is UDP. So the receiver is a plain Node `dgram` socket bound
 * to an ephemeral loopback port, and we decode the binary packets in-process
 * (see decodeCollectd below) to assert on the host/plugin fields and values.
 *
 * The collectd binary protocol is a flat stream of TLV "parts" (RFC-less, but
 * documented at https://collectd.org/wiki/index.php/Binary_protocol):
 *   - string parts (host/plugin/.../type_instance): 2-byte BE type, 2-byte BE
 *     length (incl. the 4-byte header and a trailing NUL), then the NUL-
 *     terminated string.
 *   - number parts (time/interval, "high-resolution" 2^-30 s units): same
 *     header + a single 8-byte BE uint64.
 *   - value parts: header + 2-byte BE count, then `count` 1-byte type codes
 *     (1=gauge little-endian double, 2=derive big-endian int64), then `count`
 *     8-byte values.
 * A reading is emitted whenever a value part is seen, tagged with whatever
 * host/plugin/type context the preceding string parts established. This
 * mirrors what collectd_packet.cpp on the sending side writes.
 */
import * as dgram from "dgram";
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(600_000);

// Distinctive sender hostname so the decoded `host` field has a stable anchor
// regardless of the machine the test runs on.
const HOSTNAME = "it-collectd-host";

// collectd part type codes (collectd_packet.cpp add_* helpers).
const PART_HOST = 0x0000;
const PART_TIME_HR = 0x0008;
const PART_PLUGIN = 0x0002;
const PART_PLUGIN_INSTANCE = 0x0003;
const PART_TYPE = 0x0004;
const PART_TYPE_INSTANCE = 0x0005;
const PART_VALUES = 0x0006;
const PART_INTERVAL_HR = 0x0009;

// collectd "high-resolution" time/interval are in units of 2^-30 seconds.
const HR_SHIFT = 1 << 30;

interface CollectdReading {
  host: string;
  plugin: string;
  pluginInstance: string;
  type: string;
  typeInstance: string;
  /** Number of values in the value-list. */
  count: number;
  /** Reported interval in whole seconds (time/interval are 2^-30 s units). */
  intervalSeconds: number;
}

/**
 * Decode one collectd binary packet into the list of readings (value-lists)
 * it carries. Walks the TLV part stream, tracking the current
 * host/plugin/type context, and emits a reading each time a value part is
 * seen. Returns whatever it can parse and stops at the first malformed part
 * rather than throwing — a half-captured datagram should degrade gracefully.
 */
function decodeCollectd(buf: Buffer): CollectdReading[] {
  const readings: CollectdReading[] = [];
  let host = "";
  let plugin = "";
  let pluginInstance = "";
  let type = "";
  let typeInstance = "";
  let intervalSeconds = 0;

  const readString = (body: Buffer): string => {
    // Strip the trailing NUL the sender appends.
    let end = body.length;
    while (end > 0 && body[end - 1] === 0) end--;
    return body.toString("utf8", 0, end);
  };

  let off = 0;
  while (off + 4 <= buf.length) {
    const partType = buf.readUInt16BE(off);
    const partLen = buf.readUInt16BE(off + 2);
    // length covers the 4-byte header; anything shorter or overrunning the
    // datagram is corruption — bail with what we have.
    if (partLen < 4 || off + partLen > buf.length) break;
    const body = buf.subarray(off + 4, off + partLen);
    off += partLen;

    switch (partType) {
      case PART_HOST:
        host = readString(body);
        break;
      case PART_PLUGIN:
        plugin = readString(body);
        pluginInstance = "";
        break;
      case PART_PLUGIN_INSTANCE:
        pluginInstance = readString(body);
        break;
      case PART_TYPE:
        type = readString(body);
        typeInstance = "";
        break;
      case PART_TYPE_INSTANCE:
        typeInstance = readString(body);
        break;
      case PART_TIME_HR:
        // 8-byte BE uint64; skip — only the interval is asserted on.
        break;
      case PART_INTERVAL_HR:
        if (body.length >= 8) {
          // Read as a JS number; values fit comfortably in 53 bits here.
          intervalSeconds = Math.round(Number(body.readBigUInt64BE(0)) / HR_SHIFT);
        }
        break;
      case PART_VALUES: {
        const count = body.length >= 2 ? body.readUInt16BE(0) : 0;
        readings.push({ host, plugin, pluginInstance, type, typeInstance, count, intervalSeconds });
        break;
      }
      default:
        // time / interval / unknown — context we don't assert on.
        break;
    }
  }
  return readings;
}

/** A loopback UDP collectd receiver that decodes every datagram it gets. */
class CollectdReceiver {
  private readonly socket = dgram.createSocket("udp4");
  readonly readings: CollectdReading[] = [];
  /** Raw datagrams, kept so a failing assertion can show the bytes. */
  readonly packets: Buffer[] = [];

  async start(): Promise<number> {
    await new Promise<void>((resolve, reject) => {
      this.socket.once("error", reject);
      this.socket.bind(0, "127.0.0.1", () => {
        this.socket.off("error", reject);
        resolve();
      });
    });
    this.socket.on("message", (msg) => {
      this.packets.push(msg);
      this.readings.push(...decodeCollectd(msg));
    });
    return this.socket.address().port;
  }

  async stop(): Promise<void> {
    await new Promise<void>((resolve) => this.socket.close(() => resolve()));
  }

  /** Poll until `predicate` holds over the readings collected so far. */
  async waitFor(
    predicate: (readings: CollectdReading[]) => boolean,
    timeoutMs = 60_000,
  ): Promise<boolean> {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      if (predicate(this.readings)) return true;
      await new Promise((r) => setTimeout(r, 500));
    }
    return predicate(this.readings);
  }
}

describe("CollectD integration", () => {
  let nscp: NscpInstance;
  let receiver: CollectdReceiver;

  beforeAll(async () => {
    receiver = new CollectdReceiver();
    const port = await receiver.start();

    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        // CheckSystem is the system-metrics producer (system.cpu.* /
        // system.mem.*); on Linux this build ships it as libCheckSystem.so
        // (see graphite-submit.test.ts for the naming note).
        CheckSystem: "enabled",
        CollectdClient: "enabled",
      },
      "/settings/core": {
        // Push metrics every second so the test doesn't wait the 10s default.
        "metrics interval": "1s",
      },
      "/settings/collectd/client": {
        // Pin the collectd `host` field so the decoded packets have a stable
        // anchor. A literal (non-"auto") hostname is passed through verbatim.
        hostname: HOSTNAME,
      },
      "/settings/collectd/client/targets/default": {
        // Unicast the binary protocol at our loopback receiver instead of the
        // 239.192.74.66:25826 multicast group collectd defaults to. net::parse
        // splits host:port; connection_data reads host + port from it.
        address: `127.0.0.1:${port}`,
      },
    });

    // Run the full agent so the core metrics scheduler is live.
    nscp.start();
  });

  afterAll(async () => {
    await nscp?.stop();
    await receiver?.stop();
  });

  it("sends well-formed collectd binary packets over UDP", async () => {
    const ok = await receiver.waitFor((r) => r.length > 0);
    expect(ok).toBe(true);
    // Every datagram must have decoded into at least one value-list — i.e. the
    // bytes on the wire really are the collectd binary protocol, not garbage.
    expect(receiver.packets.length).toBeGreaterThan(0);
    expect(receiver.readings.length).toBeGreaterThan(0);
    // Each value-list carries at least one value.
    expect(receiver.readings.every((v) => v.count >= 1)).toBe(true);
  });

  it("tags packets with the configured collectd host", async () => {
    await receiver.waitFor((r) => r.some((v) => v.host === HOSTNAME));
    const hosts = new Set(receiver.readings.map((v) => v.host));
    expect(hosts).toContain(HOSTNAME);
  });

  it("maps CheckSystem metrics onto collectd plugins", async () => {
    // The default mapping is platform-specific: Linux emits cpu/memory/swap,
    // Windows emits cpu/memory/processes. cpu + memory are common to both, so
    // assert those show up once metrics have flowed.
    // Wait for the per-core readings, not just the first cpu packet: the
    // ${core} variable expands against the previous metrics snapshot, so
    // cpu-0/cpu-1/… only appear from the second push onwards.
    const ok = await receiver.waitFor(
      (r) =>
        r.some((v) => v.plugin === "cpu" && /^\d+$/.test(v.pluginInstance)) &&
        r.some((v) => v.plugin === "memory"),
    );
    expect(ok).toBe(true);
    const plugins = new Set(receiver.readings.map((v) => v.plugin));
    expect(plugins).toContain("cpu");
    expect(plugins).toContain("memory");
    // Per-core CPU (cpu-0, cpu-1, …) proves the default `core` variable
    // expanded — this was broken on Linux before the per-platform defaults.
    expect(
      receiver.readings.some((v) => v.plugin === "cpu" && /^\d+$/.test(v.pluginInstance)),
    ).toBe(true);
  });

  it("reports the default 10s interval", async () => {
    await receiver.waitFor((r) => r.length > 0);
    // No `interval` configured, so the handler's default (10s) is reported.
    expect(receiver.readings.every((v) => v.intervalSeconds === 10)).toBe(true);
  });
});

// A second instance with an explicit interval and a custom metric mapping,
// proving the mapping/interval are settings-driven rather than hard-coded.
describe("CollectD configurable mapping", () => {
  let nscp: NscpInstance;
  let receiver: CollectdReceiver;

  beforeAll(async () => {
    receiver = new CollectdReceiver();
    const port = await receiver.start();

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
        // Report a non-default interval so we can distinguish it on the wire.
        interval: "7",
      },
      // A single custom mapping replacing the built-in defaults: emit one
      // gauge under the "testplugin" plugin from a metric CheckSystem always
      // produces on Linux (system.cpu.total.idle).
      "/settings/collectd/client/metrics": {
        "testplugin-/gauge-value": "gauge:system.cpu.total.idle",
      },
      "/settings/collectd/client/targets/default": {
        address: `127.0.0.1:${port}`,
      },
    });

    nscp.start();
  });

  afterAll(async () => {
    await nscp?.stop();
    await receiver?.stop();
  });

  it("uses the configured interval and custom mapping", async () => {
    const ok = await receiver.waitFor((r) => r.some((v) => v.plugin === "testplugin"));
    expect(ok).toBe(true);
    // Only the custom mapping should be present — the built-in cpu/memory
    // defaults must not be emitted once a mapping is configured.
    const plugins = new Set(receiver.readings.map((v) => v.plugin));
    expect(plugins).toContain("testplugin");
    expect(plugins).not.toContain("memory");
    expect(plugins).not.toContain("cpu");
    // The configured 7s interval is what reaches the wire.
    expect(receiver.readings.every((v) => v.intervalSeconds === 7)).toBe(true);
  });
});

// A per-target `interval` must override the client-level interval on the wire.
describe("CollectD per-target interval override", () => {
  let nscp: NscpInstance;
  let receiver: CollectdReceiver;

  beforeAll(async () => {
    receiver = new CollectdReceiver();
    const port = await receiver.start();

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
        // Client-level default — the per-target value below must win over this.
        interval: "5",
      },
      "/settings/collectd/client/targets/default": {
        address: `127.0.0.1:${port}`,
        // Per-target override.
        interval: "13",
      },
    });

    nscp.start();
  });

  afterAll(async () => {
    await nscp?.stop();
    await receiver?.stop();
  });

  it("reports the per-target interval, not the client-level one", async () => {
    await receiver.waitFor((r) => r.length > 0);
    expect(receiver.readings.length).toBeGreaterThan(0);
    expect(receiver.readings.every((v) => v.intervalSeconds === 13)).toBe(true);
  });
});
