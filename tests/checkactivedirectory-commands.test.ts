/**
 * Exercises the CheckActiveDirectory module end-to-end against the real nscp
 * binary.
 *
 * Each case runs a one-shot client query — `nscp client --module
 * CheckActiveDirectory --boot --query <cmd> ...` — which loads the module,
 * runs the check and prints the Nagios-style result line. No server/port/
 * docker needed, and the `k=v` tokens exercise the same REST-style argument
 * parsing as the web API (including valued booleans like verify=false).
 *
 * The interesting data sources (a domain controller, a live KDC) do not exist
 * on a build machine, so the suite pins what CAN be deterministic:
 *  - check_kdc is probed against a fake KDC (a Node TCP server speaking
 *    canned Kerberos: KRB-ERROR/AS-REP/garbage) and against a closed port.
 *  - check_secure_channel / check_ad_replication assert their documented
 *    contract for whichever state the host is in (domain-joined or not),
 *    never a hard error.
 *
 * The module is Windows-only, so the whole suite is skipped elsewhere.
 */
import type { AddressInfo } from "net";
import * as net from "net";
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

const onWindows = process.platform === "win32" ? describe : describe.skip;

/** A canned Kerberos message, framed with the RFC 4120 4-byte length prefix. */
function framed(message: number[]): Buffer {
  const body = Buffer.from(message);
  const header = Buffer.alloc(4);
  header.writeUInt32BE(body.length, 0);
  return Buffer.concat([header, body]);
}

// KRB-ERROR { pvno 5, msg-type 30, error-code 25 (KDC_ERR_PREAUTH_REQUIRED) } —
// what a healthy AD KDC answers an unauthenticated AS-REQ with.
const KRB_ERROR_PREAUTH = framed([
  0x7e, 0x11, 0x30, 0x0f, 0xa0, 0x03, 0x02, 0x01, 0x05, 0xa1, 0x03, 0x02, 0x01, 0x1e, 0xa6, 0x03, 0x02, 0x01, 0x19,
]);
// A (structurally minimal) AS-REP: a KDC that issued a ticket outright.
const AS_REP = framed([0x6b, 0x03, 0x30, 0x01, 0x00]);
// A non-Kerberos speaker (e.g. a TLS server squatting on the port).
const NOT_KERBEROS = framed([0x16, 0x03, 0x01, 0x00, 0x00]);

interface FakeKdc {
  port: number;
  close: () => Promise<void>;
}

/** A TCP server that answers every request with the given canned response. */
function startFakeKdc(response: Buffer): Promise<FakeKdc> {
  return new Promise((resolve) => {
    const server = net.createServer((socket) => {
      socket.on("data", () => socket.write(response));
      socket.on("error", () => socket.destroy());
    });
    server.listen(0, "127.0.0.1", () => {
      resolve({
        port: (server.address() as AddressInfo).port,
        close: () => new Promise<void>((done) => server.close(() => done())),
      });
    });
  });
}

onWindows("CheckActiveDirectory", () => {
  let nscp: NscpInstance;

  /** Run a CheckActiveDirectory query and return the combined output. */
  async function query(command: string, args: string[] = []): Promise<string> {
    const r = await nscp.run(["client", "--module", "CheckActiveDirectory", "--boot", "--query", command, ...args], {
      allowFailure: true,
    });
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  beforeAll(() => {
    nscp = new NscpInstance();
  });

  // --- check_kdc: deterministic via the fake KDC -----------------------------

  it("check_kdc reports OK when the KDC answers with KRB-ERROR preauth-required", async () => {
    const kdc = await startFakeKdc(KRB_ERROR_PREAUTH);
    try {
      const out = await query("check_kdc", ["server=127.0.0.1", `port=${kdc.port}`, "realm=EXAMPLE.TEST", "warning=none"]);
      expect(out).toMatch(/^OK/m);
      expect(out).toMatch(/KDC_ERR_PREAUTH_REQUIRED/);
    } finally {
      await kdc.close();
    }
  });

  it("check_kdc treats an outright AS-REP as a responding KDC", async () => {
    const kdc = await startFakeKdc(AS_REP);
    try {
      const out = await query("check_kdc", ["server=127.0.0.1", `port=${kdc.port}`, "realm=EXAMPLE.TEST", "warning=none"]);
      expect(out).toMatch(/^OK/m);
      expect(out).toMatch(/AS-REP/);
    } finally {
      await kdc.close();
    }
  });

  it("check_kdc goes CRITICAL when a non-Kerberos service answers", async () => {
    const kdc = await startFakeKdc(NOT_KERBEROS);
    try {
      const out = await query("check_kdc", ["server=127.0.0.1", `port=${kdc.port}`, "realm=EXAMPLE.TEST", "warning=none"]);
      expect(out).toMatch(/^CRITICAL/m);
      expect(out).toMatch(/invalid response/);
    } finally {
      await kdc.close();
    }
  });

  it("check_kdc goes CRITICAL when nothing listens on the port", async () => {
    // Port 1 is never serviced; Windows retries a refused connect internally
    // for ~2s, comfortably inside the 5s default timeout.
    const out = await query("check_kdc", ["server=127.0.0.1", "port=1", "realm=EXAMPLE.TEST"]);
    expect(out).toMatch(/^CRITICAL/m);
    expect(out).toMatch(/connect failed|timeout/);
  });

  it("check_kdc emits round-trip perfdata in ms", async () => {
    const kdc = await startFakeKdc(KRB_ERROR_PREAUTH);
    try {
      // The default warning (time > 1000) references `time`, which is what
      // makes the perf series appear; keep it and pin only critical.
      const out = await query("check_kdc", ["server=127.0.0.1", `port=${kdc.port}`, "realm=EXAMPLE.TEST"]);
      expect(out).toMatch(/'127\.0\.0\.1'=\d+ms;1000/);
      // The default critical (responding = 0) carries no `time` bound, so the
      // crit field must stay empty — a crit of 0 reads as "always critical"
      // to perfdata consumers.
      expect(out).not.toMatch(/'127\.0\.0\.1'=\d+ms;1000;/);
    } finally {
      await kdc.close();
    }
  });

  it("check_kdc timeout= is in milliseconds and bounds all probes together", async () => {
    // 127.0.0.1:1 is never serviced and Windows retries the refused connect
    // internally for ~2s (per probe), so with four servers a sequential
    // implementation needs 4 x 1500ms of deadline while the concurrent one
    // finishes in ~1.5s. The wall-clock bound is deliberately generous to
    // absorb process startup on a loaded CI machine.
    const started = Date.now();
    const out = await query("check_kdc", [
      "server=127.0.0.1",
      "server=127.0.0.2",
      "server=127.0.0.3",
      "server=127.0.0.4",
      "port=1",
      "realm=EXAMPLE.TEST",
      "timeout=1500",
    ]);
    expect(out).toMatch(/^CRITICAL/m);
    expect(out).toMatch(/timeout after 1500ms|connect failed/);
    expect(Date.now() - started).toBeLessThan(5500);
  });

  it("check_kdc omits perfdata (not a negative sample) when the host never resolves", async () => {
    // Nothing resolves, so no exchange ever starts and there is no round trip
    // to report: `time` must render as `?` and emit no perf sample rather than
    // planting a -1ms point in the series for good.
    const out = await query("check_kdc", ["server=nx.invalid.nscp-test", "realm=EXAMPLE.TEST"]);
    expect(out).toMatch(/^CRITICAL/m);
    expect(out).not.toMatch(/=-\d+ms/);
  });

  it("check_kdc rejects an oversized realm rather than sending it", async () => {
    const out = await query("check_kdc", ["server=127.0.0.1", "port=1", `realm=${"R".repeat(300)}`]);
    expect(out).toMatch(/realm= is too long/);
  });

  it("check_kdc accepts custom rendering keywords", async () => {
    const kdc = await startFakeKdc(KRB_ERROR_PREAUTH);
    try {
      const out = await query("check_kdc", [
        "server=127.0.0.1",
        `port=${kdc.port}`,
        "realm=EXAMPLE.TEST",
        "warning=none",
        "detail-syntax=${kdc} port ${port} realm ${realm}: ${response} code=${error_code}",
      ]);
      expect(out).toMatch(new RegExp(`127\\.0\\.0\\.1 port ${kdc.port} realm EXAMPLE\\.TEST: KRB-ERROR \\S+ code=25`));
    } finally {
      await kdc.close();
    }
  });

  it("check_kdc without server= reports KDC state or the no-domain guidance", async () => {
    // Domain-joined runner: probes the discovered KDC (any status word is
    // legitimate — the DC may be slow). Workgroup runner: the documented
    // guidance to pass server=/realm=. Never an argument error.
    const out = await query("check_kdc", []);
    expect(out).toMatch(/OK|WARNING|CRITICAL|Failed to locate a KDC/);
    expect(out).not.toMatch(/does not take any arguments|unknown option/i);
  });

  // --- check_secure_channel ---------------------------------------------------

  it("check_secure_channel reports the channel state or the not-joined contract", async () => {
    const out = await query("check_secure_channel", []);
    // Domain-joined: a row rendering "secure channel to <domain> via <dc>".
    // Workgroup/standalone: the documented UNKNOWN with remediation-free text.
    expect(out).toMatch(/secure channel to|not joined to a domain/i);
    expect(out).not.toMatch(/does not take any arguments|unknown option/i);
  });

  it("check_secure_channel accepts a valued boolean over the query path (verify=false)", async () => {
    // The REST transport passes booleans as `verify=false` tokens; bool_switch
    // style options reject that, so pin the contract here.
    const out = await query("check_secure_channel", ["verify=false"]);
    expect(out).toMatch(/secure channel to|not joined to a domain/i);
    expect(out).not.toMatch(/does not take any arguments/i);
  });

  it("check_secure_channel accepts its threshold keywords", async () => {
    const out = await query("check_secure_channel", ["critical=healthy = 0 or error_code != 0", "warning=none"]);
    expect(out).not.toMatch(/does not take any arguments|invalid expression|error parsing/i);
  });

  // --- check_ad_replication ----------------------------------------------------

  it("check_ad_replication reports link health or the not-a-DC contract", async () => {
    // On a DC: link summary (or the single-DC empty state). Anywhere else the
    // documented UNKNOWN names the host so fleet-wide deployment is safe.
    const out = await query("check_ad_replication", []);
    expect(out).toMatch(/replication links|replication partners|Not a domain controller/i);
    expect(out).not.toMatch(/does not take any arguments|unknown option/i);
  });

  it("check_ad_replication bounds an unreachable server= by timeout=", async () => {
    // 127.0.0.1:135 refuses immediately on a CI runner; the point is that the
    // option parses and the check returns well inside its own deadline rather
    // than blocking on the RPC bind.
    const started = Date.now();
    const out = await query("check_ad_replication", ["server=127.0.0.1", "timeout=1500"]);
    expect(out).not.toMatch(/does not take any arguments|unknown option/i);
    expect(Date.now() - started).toBeLessThan(20_000);
  });

  it("check_ad_replication accepts its threshold keywords", async () => {
    const out = await query("check_ad_replication", [
      "warning=consecutive_failures > 0",
      "critical=consecutive_failures > 4 or last_success < -24h",
    ]);
    expect(out).not.toMatch(/does not take any arguments|invalid expression|error parsing/i);
  });
});
