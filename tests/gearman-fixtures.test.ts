/**
 * GearmanClient plan, step 1: the Mod-Gearman test images and the payload
 * fixtures captured from them — no agent code involved yet.
 *
 * Three parts:
 *
 *  1. Docker-free. The payloads under modules/GearmanClient/fixtures/ were
 *     captured from the real NEB modules (ConSol mod_gearman on Naemon,
 *     nagios-mod-gearman on Nagios Core 4.5) and their send_gearman tools.
 *     The TypeScript envelope in src/gearman.ts must decrypt every one of
 *     them and, re-encrypting the decrypted text, reproduce the captured
 *     bytes exactly. That is what turns the crypto description in the plan
 *     from research into evidence, and it is what the C++ unit tests of
 *     step 2 will assert against as well.
 *
 *  2. The bare gearmand image: the fixture can submit, grab and complete a
 *     job and read the admin protocol. Tier 2 of the plan builds on this.
 *
 *  3. Both core images: a stub worker in this test grabs the checks the real
 *     core schedules for hostgroup_gearman-test, decodes them, and pushes a
 *     result back that the core files in its status file. Proves the whole
 *     path core → NEB module → gearmand → worker → check_results → core with
 *     nothing but the fixture on the worker side.
 *
 * To run part 3 against a core started by hand instead of the docker images
 * (for example the entrypoint run natively, see
 * modules/GearmanClient/fixtures/capture.sh), set
 * `NSCP_GEARMAN_LIVE=<name>:<gearmand port>:<status.dat path>`; that block
 * then runs even with NSCP_SKIP_DOCKER=1.
 */
import * as fs from "fs";
import * as path from "path";
import {
  GearmanConnection,
  GearmanPacket,
  GearmanWorker,
  GenericContainer,
  Wait,
  adminStatus,
  adminWorkers,
  decodePacket,
  decodePayload,
  decodePlainPayload,
  decryptPayload,
  dockerOrSkip,
  encodePacket,
  encodePayload,
  encryptPayload,
  formatJobText,
  formatResultText,
  gearmanKey,
  grabPayload,
  parseKeyValueText,
  parseStatusDat,
  skipDocker,
  submitCheckResult,
  trackContainerLogs,
  unescapeOutput,
  type StartedTestContainer,
} from "@fixtures/index";

jest.setTimeout(900_000);

const FIXTURES = path.resolve(__dirname, "..", "modules", "GearmanClient", "fixtures");
const KEY = fs.readFileSync(path.join(FIXTURES, "key.txt"), "utf8");
const HOSTGROUP = "gearman-test";
const QUEUE = `hostgroup_${HOSTGROUP}`;
const CORES = ["naemon", "nagios"] as const;
type Core = (typeof CORES)[number];

function fixture(name: string): string {
  return fs.readFileSync(path.join(FIXTURES, name), "latin1");
}

function sleep(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}

// ---------------------------------------------------------------------------
// 1. Captured payloads, no docker
// ---------------------------------------------------------------------------
describe("Mod-Gearman payload fixtures", () => {
  describe.each(CORES)("%s", (core: Core) => {
    const files = fs
      .readdirSync(FIXTURES)
      .filter((f) => f.startsWith(`${core}-`) && f.endsWith(".b64"));

    it("captured the five payloads", () => {
      expect(files.sort()).toEqual([
        `${core}-job-host.b64`,
        `${core}-job-service.b64`,
        `${core}-result-active-service.b64`,
        `${core}-result-passive-host.b64`,
        `${core}-result-passive-service.b64`,
      ]);
    });

    it("decrypts the host check job the NEB module scheduled", () => {
      const text = decryptPayload(fixture(`${core}-job-host.b64`), KEY);
      expect(text.startsWith("type=")).toBe(true);
      expect(text.endsWith("\n\n\n")).toBe(true);
      const job = parseKeyValueText(text);
      expect(job).toMatchObject({
        type: "host",
        result_queue: "check_results",
        target_queue: QUEUE,
        host_name: "nscp-test",
        timeout: "3",
        command_line: "check_always_ok",
      });
      expect(job.service_description).toBeUndefined();
      expect(job.core_time).toMatch(/^\d{10}\.\d{6}$/);
    });

    it("decrypts the service check job with the expanded $ARG1$ as command_line", () => {
      const job = parseKeyValueText(decryptPayload(fixture(`${core}-job-service.b64`), KEY));
      expect(job).toMatchObject({
        type: "service",
        result_queue: "check_results",
        target_queue: QUEUE,
        host_name: "nscp-test",
        service_description: "helper",
        timeout: "3",
        command_line: "check_ok message=hello",
      });
    });

    it("decrypts send_gearman's passive service result", () => {
      const r = parseKeyValueText(
        decryptPayload(fixture(`${core}-result-passive-service.b64`), KEY),
      );
      expect(r).toMatchObject({
        type: "passive",
        host_name: "nscp-test",
        service_description: "helper",
        return_code: "0",
        latency: "0.000000",
        output: "OK: hello|'time'=1ms;5;10",
      });
      expect(r.source).toMatch(/^(nagios-send-gearman|send_gearman)$/);
      expect(r.start_time).toMatch(/^\d{10}\.\d{6}$/);
      expect(r.finish_time).toBe(r.start_time);
    });

    it("decrypts send_gearman's passive host result", () => {
      const r = parseKeyValueText(decryptPayload(fixture(`${core}-result-passive-host.b64`), KEY));
      expect(r).toMatchObject({
        type: "passive",
        host_name: "nscp-test",
        return_code: "1",
        output: "WARNING: host result",
      });
      expect(r.service_description).toBeUndefined();
    });

    it("decrypts the active result and formatResultText reproduces its text byte for byte", () => {
      const text = decryptPayload(fixture(`${core}-result-active-service.b64`), KEY);
      const r = parseKeyValueText(text);
      expect(r).toMatchObject({
        type: "active",
        host_name: "nscp-test",
        service_description: "helper",
        start_time: "1757930400.000000",
        finish_time: "1757930401.000000",
        latency: "0.500000",
        return_code: "2",
        output: "CRITICAL: active result\\nsecond line|'load'=12%;80;90",
      });
      expect(unescapeOutput(r.output)).toBe(
        "CRITICAL: active result\nsecond line|'load'=12%;80;90",
      );
      // The fixed --starttime/--finishtime make this one deterministic, so
      // the TypeScript formatter can be pinned to send_gearman's exact
      // field order and number formatting.
      const rebuilt = formatResultText({
        type: "active",
        host_name: "nscp-test",
        service_description: "helper",
        start_time: "1757930400.000000",
        finish_time: "1757930401.000000",
        latency: "0.500000",
        return_code: 2,
        source: r.source,
        output: "CRITICAL: active result\nsecond line|'load'=12%;80;90",
      });
      expect(rebuilt).toBe(text);
    });

    it("formatJobText reproduces both captured job texts byte for byte", () => {
      for (const kind of ["host", "service"] as const) {
        const text = decryptPayload(fixture(`${core}-job-${kind}.b64`), KEY);
        const f = parseKeyValueText(text);
        const rebuilt = formatJobText({
          type: kind,
          host_name: f.host_name,
          service_description: f.service_description,
          result_queue: f.result_queue,
          target_queue: f.target_queue,
          core_time: f.core_time,
          timeout: Number(f.timeout),
          command_line: f.command_line,
          start_time: f.start_time,
          next_check: f.next_check,
        });
        expect(rebuilt).toBe(text);
      }
    });

    it("re-encrypting every decrypted payload reproduces the captured bytes", () => {
      for (const file of files) {
        const captured = fixture(file);
        const text = decryptPayload(captured, KEY);
        expect({ file, b64: encryptPayload(text, KEY) }).toEqual({ file, b64: captured });
      }
    });

    it("decodePayload treats an encrypted payload as encrypted even in accept-all mode", () => {
      const captured = fixture(`${core}-job-service.b64`);
      expect(decodePayload(captured, { key: KEY })).toBe(decryptPayload(captured, KEY));
      // A wrong key decrypts to garbage, never to a type= line.
      expect(decryptPayload(captured, "not-the-key").startsWith("type=")).toBe(false);
    });
  });

  it("the Nagios fork adds start_time and next_check to the job text; Naemon does not", () => {
    const nagios = parseKeyValueText(decryptPayload(fixture("nagios-job-service.b64"), KEY));
    const naemon = parseKeyValueText(decryptPayload(fixture("naemon-job-service.b64"), KEY));
    expect(nagios.start_time).toMatch(/^\d{10}\.0$/);
    expect(nagios.next_check).toMatch(/^\d{10}\.0$/);
    expect(naemon.start_time).toBeUndefined();
    expect(naemon.next_check).toBeUndefined();
    // Everything else is the same set of keys in the same order.
    const strip = (r: Record<string, string>): string[] =>
      Object.keys(r).filter((k) => k !== "start_time" && k !== "next_check");
    expect(strip(nagios)).toEqual(strip(naemon));
  });

  it("the two flavours' send_gearman differ only in the source field", () => {
    const nagios = parseKeyValueText(
      decryptPayload(fixture("nagios-result-active-service.b64"), KEY),
    );
    const naemon = parseKeyValueText(
      decryptPayload(fixture("naemon-result-active-service.b64"), KEY),
    );
    expect(nagios.source).toBe("nagios-send-gearman");
    expect(naemon.source).toBe("send_gearman");
    delete nagios.source;
    delete naemon.source;
    expect(nagios).toEqual(naemon);
  });
});

// ---------------------------------------------------------------------------
// Envelope edge cases, no docker
// ---------------------------------------------------------------------------
describe("Mod-Gearman envelope", () => {
  it("pads a short key with NULs and truncates a long one to 32 bytes", () => {
    expect(gearmanKey("abc")).toEqual(Buffer.concat([Buffer.from("abc"), Buffer.alloc(29, 0)]));
    const long = "0123456789abcdef0123456789abcdefTRAILING";
    expect(gearmanKey(long)).toEqual(Buffer.from(long.substring(0, 32)));
    const text = "type=service\nhost_name=x\n";
    expect(decryptPayload(encryptPayload(text, long), long.substring(0, 32))).toBe(text);
  });

  it("zero-pads to the AES block size and strips the padding on the way back", () => {
    // 15 characters + the terminating NUL = exactly one block, no padding.
    const one = "type=x\nhost=ab\n";
    expect(one.length).toBe(15);
    expect(Buffer.from(encryptPayload(one, KEY), "base64")).toHaveLength(16);
    expect(decryptPayload(encryptPayload(one, KEY), KEY)).toBe(one);
    // 20 characters + NUL = 21 bytes, padded to 32.
    const two = "type=service\nhost=a\n";
    expect(two.length).toBe(20);
    expect(Buffer.from(encryptPayload(two, KEY), "base64")).toHaveLength(32);
    expect(decryptPayload(encryptPayload(two, KEY), KEY)).toBe(two);
    // 31 characters + NUL = 32 bytes: gm_crypt.c's `BLOCKSIZE % len` test
    // appends a whole extra block of zeros here, which the decoder must
    // tolerate (it only ever reads up to the first NUL).
    const quirk = "type=service\nhost_name=abcdefg\n";
    expect(quirk.length).toBe(31);
    expect(Buffer.from(encryptPayload(quirk, KEY), "base64")).toHaveLength(48);
    expect(decryptPayload(encryptPayload(quirk, KEY), KEY)).toBe(quirk);
  });

  it("decrypts base64 with embedded newlines and ignores a trailing partial block", () => {
    const text = "type=passive\nhost_name=h\noutput=ok\n\n";
    const b64 = encryptPayload(text, KEY);
    const wrapped = b64.replace(/(.{20})/g, "$1\n");
    expect(decryptPayload(wrapped, KEY)).toBe(text);
    const withJunk = Buffer.concat([Buffer.from(b64, "base64"), Buffer.from("xyz")]).toString(
      "base64",
    );
    expect(decryptPayload(withJunk, KEY)).toBe(text);
  });

  it("plain mode is base64 only and is recognised by its type= prefix", () => {
    const text = "type=host\nhost_name=h\ncommand_line=check_ok\n\n\n";
    const plain = encodePayload(text, { key: KEY, encryption: false });
    expect(decodePlainPayload(plain)).toBe(text);
    expect(decodePayload(plain, { key: KEY, encryption: false })).toBe(text);
    // accept_clear_results: an encrypted-mode receiver still takes a plain
    // payload when it already reads as text.
    expect(decodePayload(plain, { key: KEY })).toBe(text);
  });
});

// ---------------------------------------------------------------------------
// Packet codec, no docker
// ---------------------------------------------------------------------------
describe("gearman packet codec", () => {
  it("round-trips JOB_ASSIGN with NULs inside the workload", () => {
    const workload = Buffer.from("a\0b\0c");
    const buf = encodePacket(
      GearmanPacket.JOB_ASSIGN,
      ["H:host:1", "hostgroup_x", workload],
      "RES",
    );
    expect(buf.subarray(0, 4)).toEqual(Buffer.from("\0RES", "latin1"));
    expect(buf.readUInt32BE(4)).toBe(11);
    expect(buf.readUInt32BE(8)).toBe("H:host:1".length + 1 + "hostgroup_x".length + 1 + 5);
    const decoded = decodePacket(buf);
    expect(decoded?.consumed).toBe(buf.length);
    expect(decoded?.packet.type).toBe(GearmanPacket.JOB_ASSIGN);
    expect(decoded?.packet.args.map((a) => a.toString("latin1"))).toEqual([
      "H:host:1",
      "hostgroup_x",
      "a\0b\0c",
    ]);
  });

  it("returns null for a truncated packet and decodes two packets back to back", () => {
    const a = encodePacket(GearmanPacket.NO_JOB, [], "RES");
    const b = encodePacket(GearmanPacket.JOB_CREATED, ["H:x:7"], "RES");
    const both = Buffer.concat([a, b]);
    expect(decodePacket(both.subarray(0, 5))).toBeNull();
    expect(decodePacket(both.subarray(0, a.length + 3))?.packet.type).toBe(GearmanPacket.NO_JOB);
    const first = decodePacket(both);
    expect(first?.consumed).toBe(a.length);
    const second = decodePacket(both.subarray(first!.consumed));
    expect(second?.packet.args[0].toString()).toBe("H:x:7");
  });

  it("rejects a bad magic and a wrong argument count", () => {
    expect(() => decodePacket(Buffer.from("XXXX\0\0\0\x01\0\0\0\0", "latin1"))).toThrow(/magic/);
    expect(() => encodePacket(GearmanPacket.CAN_DO, [])).toThrow(/takes 1 argument/);
  });
});

// ---------------------------------------------------------------------------
// 2. The gearmand image
// ---------------------------------------------------------------------------
dockerOrSkip()("gearmand image", () => {
  let gearmand: StartedTestContainer;
  let port: number;

  beforeAll(async () => {
    const image = await GenericContainer.fromDockerfile(
      path.resolve(__dirname),
      "Dockerfiles/gearmand.Dockerfile",
    ).build("nscp-it/gearmand", { deleteOnExit: false });
    gearmand = await trackContainerLogs(
      await image.withExposedPorts(4730).withWaitStrategy(Wait.forListeningPorts()).start(),
      "gearmand",
    );
    port = gearmand.getMappedPort(4730);
  });

  afterAll(async () => {
    await gearmand?.stop();
  });

  it("a worker registered through the fixture shows up in the admin protocol", async () => {
    const worker = await GearmanWorker.connect(
      "127.0.0.1",
      port,
      ["hostgroup_fixture"],
      "nscp-fixture-1",
    );
    try {
      const workers = await adminWorkers("127.0.0.1", port);
      expect(
        workers.some(
          (w) => w.clientId === "nscp-fixture-1" && w.functions.includes("hostgroup_fixture"),
        ),
      ).toBe(true);
      const status = await adminStatus("127.0.0.1", port);
      expect(status.get("hostgroup_fixture")).toEqual({ queued: 0, running: 0, workers: 1 });
    } finally {
      worker.close();
    }
  });

  it("a job submitted by the fixture is grabbed, completed and answered", async () => {
    const worker = await GearmanWorker.connect("127.0.0.1", port, ["hostgroup_fixture"]);
    const client = await GearmanConnection.connect("127.0.0.1", port);
    try {
      const text = "type=service\nhost_name=h\nservice_description=s\ncommand_line=check_ok\n\n\n";
      const answer = client.submitAndWait("hostgroup_fixture", encodePayload(text, { key: KEY }));
      const grabbed = await grabPayload(worker, { key: KEY }, 30_000);
      expect(grabbed).not.toBeNull();
      expect(grabbed!.job.func).toBe("hostgroup_fixture");
      expect(grabbed!.text).toBe(text);
      await worker.complete(grabbed!.job, "done");
      expect((await answer).toString()).toBe("done");
    } finally {
      client.close();
      worker.close();
    }
  });

  it("a background job waits in the queue until a worker asks for it", async () => {
    const client = await GearmanConnection.connect("127.0.0.1", port);
    const handle = await client.submitBackground("check_results", "payload", "unique-1");
    client.close();
    expect(handle).toMatch(/^H:/);
    expect((await adminStatus("127.0.0.1", port)).get("check_results")).toMatchObject({
      queued: 1,
    });
    const worker = await GearmanWorker.connect("127.0.0.1", port, ["check_results"]);
    try {
      const job = await worker.grab(10_000);
      expect(job?.workload.toString()).toBe("payload");
      await worker.complete(job!);
    } finally {
      worker.close();
    }
  });
});

// ---------------------------------------------------------------------------
// 3. The two core images (or a live core via NSCP_GEARMAN_LIVE)
// ---------------------------------------------------------------------------
interface CoreUnderTest {
  port: number;
  readStatusDat(): Promise<string>;
  stop(): Promise<void>;
}

interface CoreProvider {
  name: string;
  start(): Promise<CoreUnderTest>;
}

function dockerCore(core: Core): CoreProvider {
  return {
    name: core,
    async start() {
      const image = await GenericContainer.fromDockerfile(
        path.resolve(__dirname),
        `Dockerfiles/${core}-gearman.Dockerfile`,
      ).build(`nscp-it/${core}-gearman`, { deleteOnExit: false });
      const container = await trackContainerLogs(
        await image
          .withExposedPorts(4730)
          .withEnvironment({ GEARMAN_KEY: KEY, GEARMAN_HOSTGROUP: HOSTGROUP })
          .withWaitStrategy(Wait.forLogMessage(/Starting \w+ in the foreground/))
          .withStartupTimeout(180_000)
          .start(),
        `${core}-gearman`,
      );
      return {
        port: container.getMappedPort(4730),
        readStatusDat: async () =>
          (await container.exec(["cat", "/gearman-test/var/status.dat"])).output,
        stop: () => container.stop().then(() => undefined),
      };
    },
  };
}

function liveCore(spec: string): CoreProvider {
  const m = /^([^:]+):(\d+):(.+)$/.exec(spec);
  if (!m) throw new Error("NSCP_GEARMAN_LIVE must be <name>:<port>:<status.dat path>");
  return {
    name: `${m[1]} (live)`,
    async start() {
      return {
        port: Number(m[2]),
        readStatusDat: async () => fs.promises.readFile(m[3], "utf8"),
        stop: async () => undefined,
      };
    },
  };
}

const live = process.env.NSCP_GEARMAN_LIVE;
const providers: CoreProvider[] = live ? [liveCore(live)] : CORES.map(dockerCore);
const coreDescribe = live || !skipDocker() ? describe : describe.skip;

coreDescribe("a real core scheduling checks through gearmand", () => {
  describe.each(providers.map((p) => [p.name, p] as const))("%s", (_name, provider) => {
    let core: CoreUnderTest;
    let worker: GearmanWorker;
    const seen: Record<string, Record<string, string>> = {};

    beforeAll(async () => {
      core = await provider.start();
    });

    afterAll(async () => {
      worker?.close();
      await core?.stop();
    });

    it("loads the NEB module, whose result thread registers on check_results", async () => {
      const deadline = Date.now() + 60_000;
      let status = await adminStatus("127.0.0.1", core.port);
      while ((status.get("check_results")?.workers ?? 0) < 1 && Date.now() < deadline) {
        await sleep(1000);
        status = await adminStatus("127.0.0.1", core.port);
      }
      expect(status.get("check_results")?.workers).toBeGreaterThanOrEqual(1);
    });

    it(`routes the host and service checks of hostgroup ${HOSTGROUP} to the fixture worker`, async () => {
      worker = await GearmanWorker.connect("127.0.0.1", core.port, [QUEUE], "nscp-test-stub");
      const deadline = Date.now() + 120_000;
      while (
        !(seen.host && seen["service:helper"] && seen["service:slow"]) &&
        Date.now() < deadline
      ) {
        const grabbed = await grabPayload(worker, { key: KEY }, Math.max(1, deadline - Date.now()));
        if (!grabbed) break;
        const f = grabbed.fields;
        expect(f.host_name).toBe("nscp-test");
        expect(f.result_queue).toBe("check_results");
        expect(f.target_queue).toBe(QUEUE);
        expect(f.core_time).toMatch(/^\d{10}\.\d{6}$/);
        expect(f.timeout).toBe("3");
        if (f.type === "host") seen.host = f;
        else if (f.type === "service") seen[`service:${f.service_description}`] = f;
        // The result goes through check_results; WORK_COMPLETE only closes
        // the job handle, exactly as the real workers do.
        await worker.complete(grabbed.job);
      }
      expect(seen.host).toMatchObject({ type: "host", command_line: "check_always_ok" });
      expect(seen["service:helper"]).toMatchObject({
        type: "service",
        service_description: "helper",
        command_line: "check_ok message=hello",
      });
      expect(seen["service:slow"]).toMatchObject({
        type: "service",
        service_description: "slow",
        command_line: "check_timeout timeout=30",
      });
    });

    it("files a result the fixture encrypts into check_results as an active check", async () => {
      const output = `hello from the fixture ${Date.now()}|'answer'=42`;
      const now = Date.now() / 1000;
      await submitCheckResult(
        { host: "127.0.0.1", port: core.port },
        {
          type: "active",
          host_name: "nscp-test",
          service_description: "helper",
          return_code: 0,
          output,
          start_time: (now - 0.2).toFixed(6),
          finish_time: now.toFixed(6),
          source: "nscp-test-stub",
        },
        { key: KEY },
      );
      const deadline = Date.now() + 60_000;
      let service: Record<string, string> | undefined;
      while (Date.now() < deadline) {
        const status = parseStatusDat(await core.readStatusDat());
        service = status.services.get("nscp-test!helper");
        if (service?.plugin_output === output.split("|")[0]) break;
        await sleep(1000);
      }
      expect(service).toBeDefined();
      expect(service!.plugin_output).toBe(output.split("|")[0]);
      expect(service!.performance_data).toBe("'answer'=42");
      expect(service!.current_state).toBe("0");
      expect(service!.check_type).toBe("0");
    });
  });
});
