/**
 * Exercises the CheckNet module's recently-added network checks end-to-end
 * against throwaway local servers (net / tls / http / https / dgram), so the
 * suite is self-contained apart from the "against the public internet" block at
 * the end, which is the only place the platform's own CA bundle is exercised:
 *
 *   - check_tcp   — plain + TLS connect, greeting expect, connection refused
 *   - check_ssh   — SSH banner validation
 *   - check_http  — HTTP method, Basic auth, redirect following, chunked
 *                   de-chunk, TLS + ssl_expiry_days certificate keyword
 *   - check_dns   — record-type query against a chosen server (the UDP client)
 *   - check_nsclient_web_online — REST reachability + remote-check passthrough
 *
 * Queries run over the REST API against a long-lived `nscp test` process (see
 * setupQueryNscp). Every check *target* is a separate Node listener on loopback,
 * so a check never re-enters nscp's own WEB server (which would deadlock).
 */
import * as dgram from "dgram";
import * as http from "http";
import * as https from "https";
import type { AddressInfo } from "net";
import * as net from "net";
import * as tls from "tls";

import {
  CRITICAL,
  NscpInstance,
  OK,
  UNKNOWN,
  WARNING,
  type CertPair,
  executeQuery,
  generateCertChain,
  messageOf,
  perfOf,
  perfValue,
  setupQueryNscp,
} from "@fixtures/index";

jest.setTimeout(120_000);

interface Listener {
  port: number;
  close: () => Promise<void>;
}

/** Servers opened by the current test, torn down in afterEach. */
const openListeners: Listener[] = [];

function track(l: Listener): Listener {
  openListeners.push(l);
  return l;
}

function portOf(srv: net.Server): number {
  return (srv.address() as AddressInfo).port;
}

function closeNetServer(srv: net.Server): () => Promise<void> {
  return () => new Promise<void>((res) => srv.close(() => res()));
}

/**
 * A plain-TCP server that greets each client and closes (FTP/SMTP style).
 * `bind` selects the loopback address, so passing "::1" gives a listener that
 * only an IPv6 connection can reach - which is how the address-family tests
 * prove the flag actually changed the transport.
 */
function startTcpGreeter(greeting: string, bind = "127.0.0.1"): Promise<Listener> {
  return new Promise((resolve) => {
    const srv = net.createServer((sock) => {
      sock.on("error", () => {});
      sock.end(greeting); // write banner then FIN → the check sees a clean EOF
    });
    srv.listen(0, bind, () => resolve(track({ port: portOf(srv), close: closeNetServer(srv) })));
  });
}

/** A TLS server that greets over the encrypted channel then closes. */
function startTlsGreeter(greeting: string, cert: CertPair): Promise<Listener> {
  return new Promise((resolve) => {
    const srv = tls.createServer({ key: cert.keyPem, cert: cert.certPem }, (sock) => {
      sock.on("error", () => {});
      sock.end(greeting);
    });
    srv.listen(0, "127.0.0.1", () =>
      resolve(
        track({
          port: portOf(srv as unknown as net.Server),
          close: closeNetServer(srv as unknown as net.Server),
        }),
      ),
    );
  });
}

/** An http or https server driven by the supplied request handler. */
function startHttp(
  handler: http.RequestListener,
  cert?: CertPair,
  bind = "127.0.0.1",
): Promise<Listener> {
  return new Promise((resolve) => {
    const srv = cert
      ? https.createServer({ key: cert.keyPem, cert: cert.certPem }, handler)
      : http.createServer(handler);
    srv.listen(0, bind, () =>
      resolve(
        track({
          port: portOf(srv as unknown as net.Server),
          close: closeNetServer(srv as unknown as net.Server),
        }),
      ),
    );
  });
}

/**
 * A minimal UDP DNS responder: echoes the query's transaction id + question and
 * answers every query with a single A record for the given IPv4 address. Enough
 * to exercise check_dns's DNS-over-UDP client and A-record parser end to end.
 */
function startDnsResponder(
  ip: [number, number, number, number] = [93, 184, 216, 34],
  bind = "127.0.0.1",
): Promise<Listener> {
  return new Promise((resolve) => {
    const sock = dgram.createSocket(bind.includes(":") ? "udp6" : "udp4");
    sock.on("message", (msg, rinfo) => {
      // Question starts at offset 12; walk the length-prefixed labels to the 0.
      let p = 12;
      while (p < msg.length && msg[p] !== 0) p += msg[p] + 1;
      const qEnd = p + 1 + 4; // terminating 0 + qtype(2) + qclass(2)
      const question = msg.subarray(12, qEnd);

      const header = Buffer.alloc(12);
      msg.copy(header, 0, 0, 2); // transaction id
      header.writeUInt16BE(0x8180, 2); // QR=1, RD=1, RA=1, rcode=0
      header.writeUInt16BE(1, 4); // qdcount
      header.writeUInt16BE(1, 6); // ancount

      const answer = Buffer.from([
        0xc0,
        0x0c, // name → compression pointer to the question at offset 12
        0x00,
        0x01,
        0x00,
        0x01, // type A, class IN
        0x00,
        0x00,
        0x00,
        0x3c, // ttl 60
        0x00,
        0x04, // rdlength
        ip[0],
        ip[1],
        ip[2],
        ip[3],
      ]);
      sock.send(Buffer.concat([header, question, answer]), rinfo.port, rinfo.address);
    });
    sock.bind(0, bind, () =>
      resolve(
        track({
          port: (sock.address() as AddressInfo).port,
          close: () => new Promise<void>((r) => sock.close(() => r())),
        }),
      ),
    );
  });
}

interface NtpOptions {
  stratum?: number;
  bind?: string;
  /**
   * Milliseconds added to the served time, cycled per request. A varying series
   * produces jitter; a constant one produces a steady offset with no jitter.
   */
  skewSequenceMs?: number[];
  /** Root delay / root dispersion to advertise, in milliseconds. */
  rootDelayMs?: number;
  rootDispersionMs?: number;
}

/**
 * A minimal UDP NTP responder: answers every request with a server-mode packet
 * carrying the current time at the given stratum. Enough to exercise
 * check_ntp_offset's client end to end without touching a real time server.
 */
function startNtpResponder(opts: NtpOptions = {}): Promise<Listener> {
  const stratum = opts.stratum ?? 2;
  const bind = opts.bind ?? "127.0.0.1";
  const skews = opts.skewSequenceMs ?? [0];
  let call = 0;
  return new Promise((resolve) => {
    const sock = dgram.createSocket(bind.includes(":") ? "udp6" : "udp4");
    sock.on("message", (_msg, rinfo) => {
      const pkt = Buffer.alloc(48);
      pkt[0] = 0x1c; // LI=0, VN=3, mode=4 (server)
      pkt[1] = stratum;
      // Root delay (byte 4) and dispersion (byte 8) are 16.16 fixed-point secs.
      const toShort = (ms: number) => Math.round((ms / 1000) * 65536);
      pkt.writeUInt32BE(toShort(opts.rootDelayMs ?? 0), 4);
      pkt.writeUInt32BE(toShort(opts.rootDispersionMs ?? 0), 8);
      // NTP timestamps are seconds since 1900 plus a 32-bit fraction.
      const skew = skews[call % skews.length];
      call += 1;
      const nowMs = Date.now() + skew;
      const secs = Math.floor(nowMs / 1000) + 2208988800;
      const frac = Math.floor(((nowMs % 1000) / 1000) * 4294967296);
      for (const off of [16, 24, 32, 40]) {
        pkt.writeUInt32BE(secs, off);
        pkt.writeUInt32BE(frac, off + 4);
      }
      sock.send(pkt, rinfo.port, rinfo.address);
    });
    sock.bind(0, bind, () =>
      resolve(
        track({
          port: (sock.address() as AddressInfo).port,
          close: () => new Promise<void>((r) => sock.close(() => r())),
        }),
      ),
    );
  });
}

/** A free loopback port with nothing listening (for the refused-connection case). */
function closedPort(): Promise<number> {
  return new Promise((resolve) => {
    const srv = net.createServer();
    srv.listen(0, "127.0.0.1", () => {
      const port = portOf(srv);
      srv.close(() => resolve(port));
    });
  });
}

describe("CheckNet commands", () => {
  let nscp: NscpInstance;
  let key: string;
  let serverCert: CertPair;

  beforeAll(async () => {
    nscp = new NscpInstance();
    // Self-signed cert with SAN DNS:localhost + IP:127.0.0.1 (365-day validity)
    // used by the TLS test servers below.
    serverCert = generateCertChain({
      outDir: nscp.scratch("checknet_certs"),
      signed: { server: { commonName: "localhost", isServer: true } },
    }).signed.server;
    key = await setupQueryNscp(nscp, "CheckNet");
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  afterEach(async () => {
    await Promise.all(openListeners.splice(0).map((l) => l.close()));
  });

  // --- check_tcp ------------------------------------------------------------

  it("check_tcp connects and matches the greeting", async () => {
    const s = await startTcpGreeter("220 service ready\r\n");
    const q = await executeQuery(key, "check_tcp", {
      host: "127.0.0.1",
      port: String(s.port),
      expect: "220",
    });
    expect(q.result).toBe(OK);
  });

  it("check_tcp reports no_match when the greeting fails expect", async () => {
    const s = await startTcpGreeter("500 nope\r\n");
    const q = await executeQuery(key, "check_tcp", {
      host: "127.0.0.1",
      port: String(s.port),
      expect: "220",
    });
    expect(q.result).toBe(CRITICAL);
    expect(messageOf(q)).toMatch(/no_match/);
  });

  it("check_tcp reports a refused connection", async () => {
    const port = await closedPort();
    const q = await executeQuery(key, "check_tcp", { host: "127.0.0.1", port: String(port) });
    expect(q.result).toBe(CRITICAL);
    expect(messageOf(q)).toMatch(/refused/);
  });

  it("check_tcp --ssl completes a TLS handshake and matches the greeting", async () => {
    const s = await startTlsGreeter("220 secure service\r\n", serverCert);
    const q = await executeQuery(key, "check_tcp", {
      host: "127.0.0.1",
      port: String(s.port),
      ssl: "true",
      verify: "none",
      expect: "220",
    });
    expect(q.result).toBe(OK);
  });

  it("check_tcp exposes the peer certificate expiry via ssl_expiry_days", async () => {
    const s = await startTlsGreeter("220 secure service\r\n", serverCert);
    const q = await executeQuery(key, "check_tcp", {
      host: "127.0.0.1",
      port: String(s.port),
      ssl: "true",
      verify: "none",
      "top-syntax": "${list}",
      "detail-syntax": "cert=${has_certificate} days=${ssl_expiry_days}",
    });
    expect(q.result).toBe(OK);
    const m = messageOf(q).match(/cert=(\d+) days=(\d+)/);
    expect(m).not.toBeNull();
    expect(Number(m?.[1])).toBe(1); // a certificate was presented
    // The shared fixture cert is valid for 365 days.
    expect(Number(m?.[2])).toBeGreaterThan(300);
  });

  it("check_tcp reads the certificate without verifying it", async () => {
    // verify=none is the default: the expiry is a property of the certificate
    // the peer served, so it must be readable without a trust decision (the
    // fixture cert is signed by a CA nscp does not trust).
    const s = await startTlsGreeter("220 secure service\r\n", serverCert);
    const q = await executeQuery(key, "check_tcp", {
      host: "127.0.0.1",
      port: String(s.port),
      ssl: "true",
      critical: "has_certificate = 0",
    });
    expect(q.result).toBe(OK);
  });

  it("check_tcp alerts on a certificate that expires soon", async () => {
    // A 20-day cert against a 30-day threshold: this is the check the keyword
    // exists for, and it must fire on the real remaining lifetime.
    const shortLived = generateCertChain({
      outDir: nscp.scratch("checknet_shortlived"),
      signed: { server: { commonName: "localhost", isServer: true } },
      days: 20,
    }).signed.server;
    const s = await startTlsGreeter("220 secure service\r\n", shortLived);
    const q = await executeQuery(key, "check_tcp", {
      host: "127.0.0.1",
      port: String(s.port),
      ssl: "true",
      critical: "has_certificate = 1 and ssl_expiry_days < 30",
      "top-syntax": "${list}",
      "detail-syntax": "expires in ${ssl_expiry_days} days",
    });
    expect(q.result).toBe(CRITICAL);
    // Whole days, with the sub-day remainder dropped, so a cert issued for 20
    // days reads as 20 or 19 depending on where in the second the check lands.
    const days = Number(messageOf(q).match(/expires in (\d+) days/)?.[1]);
    expect(days).toBeGreaterThanOrEqual(19);
    expect(days).toBeLessThanOrEqual(20);
  });

  it("check_tcp emits ssl_expiry_days as perfdata when thresholded", async () => {
    const s = await startTlsGreeter("220 secure service\r\n", serverCert);
    const q = await executeQuery(key, "check_tcp", {
      host: "127.0.0.1",
      port: String(s.port),
      ssl: "true",
      warning: "ssl_expiry_days < 30",
    });
    expect(q.result).toBe(OK);
    expect(perfValue(q, "127.0.0.1_" + s.port + "_ssl_expiry_days")).toBeGreaterThan(300);
  });

  it("check_tcp reports no certificate on a plain connection", async () => {
    // Without ssl=true there is no certificate at all: has_certificate must be
    // 0 so a threshold can tell that apart from an expired one (both of which
    // would otherwise look like a negative ssl_expiry_days).
    const s = await startTcpGreeter("220 service ready\r\n");
    const q = await executeQuery(key, "check_tcp", {
      host: "127.0.0.1",
      port: String(s.port),
      "top-syntax": "${list}",
      "detail-syntax": "cert=${has_certificate} days=${ssl_expiry_days}",
    });
    expect(q.result).toBe(OK);
    // The optional number renders its no-value string on a plain connection.
    expect(messageOf(q)).toBe("cert=0 days=no certificate");
  });

  it("check_tcp expiry thresholds cannot fire on a plain connection", async () => {
    // The trap this feature removes: ssl_expiry_days < 30 used to be true on
    // every non-TLS connection (the -1 sentinel), forcing a has_certificate
    // guard. A missing value now compares false to every number, so the bare
    // threshold is safe; the string form is the explicit presence test.
    const s = await startTcpGreeter("220 service ready\r\n");
    const bare = await executeQuery(key, "check_tcp", {
      host: "127.0.0.1",
      port: String(s.port),
      warning: "none",
      critical: "ssl_expiry_days < 30",
    });
    expect(bare.result).toBe(OK);

    const presence = await executeQuery(key, "check_tcp", {
      host: "127.0.0.1",
      port: String(s.port),
      warning: "none",
      critical: "ssl_expiry_days = 'no certificate'",
    });
    expect(presence.result).toBe(CRITICAL);
  });

  it("check_tcp certificate keywords work through a service preset", async () => {
    // The s-prefixed presets (spop/simap/ssmtp) imply TLS, so they get the
    // certificate keywords without ssl=true being passed explicitly.
    const s = await startTlsGreeter("+OK ready\r\n", serverCert);
    const q = await executeQuery(key, "check_tcp", {
      host: "127.0.0.1",
      port: String(s.port),
      service: "spop",
      "top-syntax": "${list}",
      "detail-syntax": "cert=${has_certificate}",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toBe("cert=1");
  });

  // --- check_ssh ------------------------------------------------------------

  it("check_ssh accepts a valid SSH banner", async () => {
    const s = await startTcpGreeter("SSH-2.0-TestServer\r\n");
    const q = await executeQuery(key, "check_ssh", { host: "127.0.0.1", port: String(s.port) });
    expect(q.result).toBe(OK);
  });

  it("check_ssh rejects a non-SSH banner", async () => {
    const s = await startTcpGreeter("HELLO not ssh\r\n");
    const q = await executeQuery(key, "check_ssh", { host: "127.0.0.1", port: String(s.port) });
    expect(q.result).toBe(CRITICAL);
  });

  it("check_ssh splits the identification string into keywords", async () => {
    const s = await startTcpGreeter("SSH-2.0-OpenSSH_9.6p1 Ubuntu-3ubuntu13.5\r\n");
    const q = await executeQuery(key, "check_ssh", {
      host: "127.0.0.1",
      port: String(s.port),
      "top-syntax": "${list}",
      "detail-syntax":
        "proto=${protocol} major=${protocol_major} minor=${protocol_minor} version=${version} sw=${software} swver=${software_version} comments=[${comments}]",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toBe(
      "proto=2.0 major=2 minor=0 version=OpenSSH_9.6p1 sw=OpenSSH swver=9.6p1 comments=[Ubuntu-3ubuntu13.5]",
    );
  });

  it("check_ssh banner keyword carries the raw identification string", async () => {
    const s = await startTcpGreeter("SSH-2.0-OpenSSH_9.6p1\r\n");
    const q = await executeQuery(key, "check_ssh", {
      host: "127.0.0.1",
      port: String(s.port),
      "top-syntax": "${list}",
      "detail-syntax": "${banner}",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toBe("SSH-2.0-OpenSSH_9.6p1");
  });

  it("check_ssh can alert on an outdated software version", async () => {
    // The point of the software/version keywords: express "not this build"
    // without hand-writing a regex over the raw banner.
    const s = await startTcpGreeter("SSH-2.0-OpenSSH_7.4\r\n");
    const q = await executeQuery(key, "check_ssh", {
      host: "127.0.0.1",
      port: String(s.port),
      critical: "software = 'OpenSSH' and software_version not like '9.'",
      "top-syntax": "${list}",
      "detail-syntax": "${software} ${software_version} is outdated",
    });
    expect(q.result).toBe(CRITICAL);
    expect(messageOf(q)).toBe("OpenSSH 7.4 is outdated");
  });

  it("check_ssh protocol_major flags an SSHv1-capable server", async () => {
    // "1.99" means the server still speaks the insecure SSHv1.
    const legacy = await startTcpGreeter("SSH-1.99-OpenSSH_3.9p1\r\n");
    const q = await executeQuery(key, "check_ssh", {
      host: "127.0.0.1",
      port: String(legacy.port),
      critical: "protocol_major < 2",
      "top-syntax": "${list}",
      "detail-syntax": "${host} speaks SSH ${protocol}",
    });
    expect(q.result).toBe(CRITICAL);
    expect(messageOf(q)).toBe("127.0.0.1 speaks SSH 1.99");

    // ...and a 2.0-only server passes the same threshold.
    const modern = await startTcpGreeter("SSH-2.0-OpenSSH_9.6p1\r\n");
    const ok = await executeQuery(key, "check_ssh", {
      host: "127.0.0.1",
      port: String(modern.port),
      critical: "protocol_major < 2",
    });
    expect(ok.result).toBe(OK);
  });

  it("check_ssh leaves the banner keywords empty when nothing was read", async () => {
    // Nothing is listening: the check fails on `result`, and the banner
    // keywords must be empty rather than stale.
    const dead = await startTcpGreeter("SSH-2.0-OpenSSH_9.6p1\r\n");
    const port = dead.port;
    await dead.close();
    const q = await executeQuery(key, "check_ssh", {
      host: "127.0.0.1",
      port: String(port),
      "top-syntax": "${list}",
      "detail-syntax": "result=${result} sw=[${software}] major=${protocol_major}",
    });
    expect(q.result).toBe(CRITICAL);
    expect(messageOf(q)).toBe("result=refused sw=[] major=0");
  });

  it("check_ssh still supports the check_tcp keywords", async () => {
    const s = await startTcpGreeter("SSH-2.0-OpenSSH_9.6p1\r\n");
    const q = await executeQuery(key, "check_ssh", {
      host: "127.0.0.1",
      port: String(s.port),
      "top-syntax": "${list}",
      "detail-syntax": "${host}:${port} ${result} connected=${connected}",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toBe(`127.0.0.1:${s.port} ok connected=1`);
  });

  // --- check_http -----------------------------------------------------------

  it("check_http reports OK for a 200 response", async () => {
    const s = await startHttp((_req, res) => {
      res.writeHead(200, { "Content-Type": "text/plain" });
      res.end("ok");
    });
    const q = await executeQuery(key, "check_http", { url: `http://127.0.0.1:${s.port}/` });
    expect(q.result).toBe(OK);
  });

  it("check_http --method sends the requested verb", async () => {
    // 200 only for HEAD, 400 otherwise: proves the verb reached the server.
    const s = await startHttp((req, res) => {
      res.writeHead(req.method === "HEAD" ? 200 : 400);
      res.end();
    });
    const head = await executeQuery(key, "check_http", {
      url: `http://127.0.0.1:${s.port}/`,
      method: "HEAD",
    });
    expect(head.result).toBe(OK);
    const get = await executeQuery(key, "check_http", { url: `http://127.0.0.1:${s.port}/` });
    expect(get.result).toBe(CRITICAL);
  });

  it("check_http performs HTTP Basic authentication", async () => {
    const s = await startHttp((req, res) => {
      if (req.headers.authorization === "Basic " + Buffer.from("foo:bar").toString("base64")) {
        res.writeHead(200);
        res.end("authorized");
      } else {
        res.writeHead(401);
        res.end("denied");
      }
    });
    const denied = await executeQuery(key, "check_http", { url: `http://127.0.0.1:${s.port}/` });
    expect(denied.result).toBe(CRITICAL);

    const ok = await executeQuery(key, "check_http", {
      url: `http://127.0.0.1:${s.port}/`,
      username: "foo",
      password: "bar",
    });
    expect(ok.result).toBe(OK);
  });

  it("check_http follows redirects only with --onredirect follow", async () => {
    const s = await startHttp((req, res) => {
      if (req.url === "/final") {
        res.writeHead(200);
        res.end("arrived");
      } else {
        res.writeHead(302, { Location: "/final" });
        res.end();
      }
    });
    // Default: the 302 is reported as-is (still OK, but the code is 302).
    const noFollow = await executeQuery(key, "check_http", {
      url: `http://127.0.0.1:${s.port}/`,
      "detail-syntax": "code=${code}",
    });
    expect(messageOf(noFollow)).toMatch(/code=302/);

    // follow: we chase the Location to the final 200.
    const follow = await executeQuery(key, "check_http", {
      url: `http://127.0.0.1:${s.port}/`,
      onredirect: "follow",
      "detail-syntax": "code=${code}",
    });
    expect(messageOf(follow)).toMatch(/code=200/);
  });

  it("check_http de-chunks a Transfer-Encoding: chunked body", async () => {
    // Two res.write() calls with no Content-Length ⇒ Node frames the body as
    // chunked. If the check did NOT de-chunk, hex size markers would split the
    // string and the substring match would fail.
    const s = await startHttp((_req, res) => {
      res.writeHead(200, { "Content-Type": "text/plain" });
      res.write("hello-");
      res.write("chunked");
      res.end();
    });
    const q = await executeQuery(key, "check_http", {
      url: `http://127.0.0.1:${s.port}/`,
      "expected-body": "hello-chunked",
    });
    expect(q.result).toBe(OK);
  });

  it("check_http over TLS exposes the certificate expiry via ssl_expiry_days", async () => {
    const s = await startHttp((_req, res) => {
      res.writeHead(200);
      res.end("secure");
    }, serverCert);
    const q = await executeQuery(key, "check_http", {
      url: `https://127.0.0.1:${s.port}/`,
      verify: "none",
      critical: "ssl_expiry_days < 0", // an in-date cert must not trip this
      "detail-syntax": "days=${ssl_expiry_days}",
    });
    expect(q.result).toBe(OK);
    const m = messageOf(q).match(/days=(\d+)/);
    expect(m).not.toBeNull();
    // The generated cert is valid for 365 days, so a few hundred days remain.
    expect(Number(m?.[1])).toBeGreaterThan(300);
  });

  it("check_http reports no certificate when a redirect lands on plain http", async () => {
    // The trap: the TLS hop presents a certificate, the hop actually checked
    // does not. Carrying the first hop's expiry forward would report a
    // certificate for a URL that never presented one - and would make
    // `ssl_expiry_days = 'no certificate'` false on a plain-http endpoint.
    const plain = await startHttp((_req, res) => {
      res.writeHead(200);
      res.end("arrived");
    });
    const tls = await startHttp((_req, res) => {
      res.writeHead(302, { Location: `http://127.0.0.1:${plain.port}/final` });
      res.end();
    }, serverCert);

    const q = await executeQuery(key, "check_http", {
      url: `https://127.0.0.1:${tls.port}/`,
      verify: "none",
      onredirect: "follow",
      "detail-syntax": "code=${code} days=${ssl_expiry_days}",
    });
    // check_http's default top-syntax prefixes the status word, so assert the
    // rendering and the status separately rather than pinning the whole line.
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toContain("code=200 days=no certificate");

    // ...and the numeric threshold that the optional number exists to protect
    // must not fire on the plain hop either.
    const bare = await executeQuery(key, "check_http", {
      url: `https://127.0.0.1:${tls.port}/`,
      verify: "none",
      onredirect: "follow",
      warning: "none",
      critical: "ssl_expiry_days < 3650",
    });
    expect(bare.result).toBe(OK);
  });

  it("check_http exposes the HTTP status message as status_message (status stays a deprecated alias)", async () => {
    // The record keyword was renamed from `status`, which clashed with the
    // generic status summary keyword (and shadowed it in filters); the old name
    // stays registered as a deprecated alias so existing filters keep working.
    const s = await startHttp((_req, res) => {
      res.writeHead(200, { "Content-Type": "text/plain" });
      res.end("ok");
    });
    const q = await executeQuery(key, "check_http", {
      url: `http://127.0.0.1:${s.port}/`,
      "top-syntax": "${list}",
      "detail-syntax": "msg=[${status_message}] alias=[${status}]",
    });
    expect(q.result).toBe(OK);
    // The status-line reason phrase is passed through verbatim (including the
    // separator whitespace around it), so compare the two renderings to each
    // other and only pin the trimmed value.
    const m = messageOf(q).match(/msg=\[([\s\S]*?)\] alias=\[([\s\S]*?)\]/);
    expect(m).not.toBeNull();
    expect(m?.[1].trim()).toBe("OK");
    expect(m?.[2]).toBe(m?.[1]);
  });

  it("check_http emits response-time, status-code and size perfdata", async () => {
    const s = await startHttp((_req, res) => {
      res.writeHead(200, { "Content-Type": "text/plain" });
      res.end("hello");
    });
    const q = await executeQuery(key, "check_http", { url: `http://127.0.0.1:${s.port}/` });
    expect(q.result).toBe(OK);
    // Previously these checks emitted no perfdata at all (issue: no graphs).
    expect(perfValue(q, `http://127.0.0.1:${s.port}/`)).toBeGreaterThanOrEqual(0); // time
    expect(perfValue(q, `http://127.0.0.1:${s.port}/_code`)).toBe(200);
    expect(perfValue(q, `http://127.0.0.1:${s.port}/_size`)).toBe(5);
  });

  // --- check_http --json-path ----------------------------------------------

  it("check_http extracts a numeric JSON value and thresholds on it", async () => {
    const s = await startHttp((_req, res) => {
      res.writeHead(200, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ data: { queue: { length: 150 } } }));
    });
    const crit = await executeQuery(key, "check_http", {
      url: `http://127.0.0.1:${s.port}/`,
      "json-path": "qlen:data.queue.length",
      critical: "qlen > 100",
    });
    expect(crit.result).toBe(CRITICAL);
    // The extracted value is emitted as perfdata.
    expect(perfValue(crit, `qlen_http://127.0.0.1:${s.port}/`)).toBe(150);

    const ok = await executeQuery(key, "check_http", {
      url: `http://127.0.0.1:${s.port}/`,
      "json-path": "qlen:data.queue.length",
      critical: "qlen > 1000",
    });
    expect(ok.result).toBe(OK);
  });

  it("check_http matches a string JSON value and honours float precision", async () => {
    const s = await startHttp((_req, res) => {
      res.writeHead(200, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ status: "degraded", ratio: 0.25 }));
    });
    // String comparison: status != 'ok' must trip CRITICAL.
    const str = await executeQuery(key, "check_http", {
      url: `http://127.0.0.1:${s.port}/`,
      "json-path": "st:status",
      critical: "st != 'ok'",
    });
    expect(str.result).toBe(CRITICAL);

    // Float threshold: 0.25 > 0.1 (would be lost if the value were truncated to int).
    const flt = await executeQuery(key, "check_http", {
      url: `http://127.0.0.1:${s.port}/`,
      "json-path": "ratio:ratio",
      critical: "ratio > 0.1",
    });
    expect(flt.result).toBe(CRITICAL);
  });

  it("check_http indexes into JSON arrays and quotes dotted keys", async () => {
    const s = await startHttp((_req, res) => {
      res.writeHead(200, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ items: [{ n: "a" }, { n: "b" }], "a.b": { c: 7 } }));
    });
    const q = await executeQuery(key, "check_http", {
      url: `http://127.0.0.1:${s.port}/`,
      "json-path": "second:items.1.n",
      "detail-syntax": "v=${second}",
      "top-syntax": "${list}",
    });
    expect(messageOf(q)).toMatch(/v=b/);
  });

  // --- check_dns ------------------------------------------------------------

  it("check_dns queries a chosen server over UDP and parses the A record", async () => {
    const s = await startDnsResponder([93, 184, 216, 34]);
    const q = await executeQuery(key, "check_dns", {
      host: "example.com",
      type: "A",
      server: "127.0.0.1",
      port: String(s.port),
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/93\.184\.216\.34/);
  });

  // Regression: `norecursion` is a boolean option passed over REST as the token
  // `norecursion=true`. It must be po::value<bool>()->implicit_value(true), not
  // po::bool_switch, which rejects a value with "option '--norecursion' does not
  // take any arguments". See docs/design/icinga-windows-parity.md §4.2.
  it("check_dns accepts norecursion=true as a valued boolean over REST", async () => {
    const s = await startDnsResponder([93, 184, 216, 34]);
    const q = await executeQuery(key, "check_dns", {
      host: "example.com",
      type: "A",
      server: "127.0.0.1",
      port: String(s.port),
      norecursion: "true",
    });
    expect(messageOf(q)).not.toMatch(/does not take any arguments/);
    expect(q.result).toBe(OK);
  });

  it("check_dns exposes the record count as records (count stays a deprecated alias)", async () => {
    // Renamed from `count`, which clashed with the generic count summary
    // keyword; the old name stays registered as a deprecated alias.
    const s = await startDnsResponder([93, 184, 216, 34]);
    const q = await executeQuery(key, "check_dns", {
      host: "example.com",
      type: "A",
      server: "127.0.0.1",
      port: String(s.port),
      critical: "records < 1",
      "top-syntax": "${list}",
      "detail-syntax": "records=${records} alias=${count}",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toBe("records=1 alias=1");
  });

  // --- check_connections ------------------------------------------------------

  it("check_connections reports the total bucket via connections/total_connections", async () => {
    // `count`/`total` clashed with the generic summary keywords, so the record
    // keywords are `connections`/`total_connections`; the old names remain as
    // deprecated aliases. Thresholds are pinned so live host state cannot flip
    // the result: the nscp REST connection itself guarantees at least one
    // connection exists.
    const q = await executeQuery(key, "check_connections", {
      warning: "none",
      critical: "total_connections < 1",
      "top-syntax": "${list}",
      "detail-syntax": "c=${connections} t=${total_connections} ac=${count} at=${total}",
    });
    expect(q.result).toBe(OK);
    const m = messageOf(q).match(/^c=(\d+) t=(\d+) ac=(\d+) at=(\d+)$/);
    expect(m).not.toBeNull();
    // On the default 'total' bucket the per-bucket count equals the total, and
    // each deprecated alias must render the same value as its new name.
    expect(Number(m?.[1])).toBeGreaterThan(0);
    expect(m?.[2]).toBe(m?.[1]);
    expect(m?.[3]).toBe(m?.[1]);
    expect(m?.[4]).toBe(m?.[1]);
  });

  // --- check_ntp_offset -----------------------------------------------------

  it("check_ntp_offset reports the server's advertised root delay and dispersion", async () => {
    // These come straight out of the packet header, so they are available from
    // a single sample and say what the server claims about its own accuracy.
    const s = await startNtpResponder({ rootDelayMs: 12, rootDispersionMs: 34 });
    const q = await executeQuery(key, "check_ntp_offset", {
      server: "127.0.0.1",
      port: String(s.port),
      "top-syntax": "${list}",
      "detail-syntax": "delay=${root_delay} disp=${root_dispersion}",
    });
    expect(q.result).toBe(OK);
    // The wire format is 16.16 fixed point, so the millisecond value is
    // truncated on the way back out.
    expect(messageOf(q)).toBe("delay=11 disp=33");
  });

  it("check_ntp_offset reports jitter as unmeasured with a single sample", async () => {
    const s = await startNtpResponder();
    const q = await executeQuery(key, "check_ntp_offset", {
      server: "127.0.0.1",
      port: String(s.port),
      "top-syntax": "${list}",
      "detail-syntax": "samples=${samples} jitter=${jitter}",
    });
    expect(q.result).toBe(OK);
    // One sample is the default, and jitter needs two to mean anything: the
    // optional number renders its no-value string, and no jitter perfdata is
    // emitted (a sentinel would poison the series).
    expect(messageOf(q)).toBe("samples=1 jitter=unknown");
    expect(perfOf(q)["127.0.0.1_jitter"]).toBeUndefined();
  });

  it("check_ntp_offset jitter = 'unknown' is the presence test", async () => {
    const s = await startNtpResponder();
    // Unmeasured: the string comparison fires...
    const unmeasured = await executeQuery(key, "check_ntp_offset", {
      server: "127.0.0.1",
      port: String(s.port),
      warning: "none",
      critical: "jitter = 'unknown'",
    });
    expect(unmeasured.result).toBe(CRITICAL);
    // ...and numeric thresholds cannot: jitter >= 0 matches every measured
    // value but must not match "unknown" (this used to be the -1 trap).
    const numeric = await executeQuery(key, "check_ntp_offset", {
      server: "127.0.0.1",
      port: String(s.port),
      warning: "jitter >= 0",
      critical: "none",
    });
    expect(numeric.result).toBe(OK);
    // With samples the same expressions flip: measured jitter is a number.
    const measured = await executeQuery(key, "check_ntp_offset", {
      server: "127.0.0.1",
      port: String(s.port),
      samples: "5",
      warning: "jitter >= 0",
      critical: "jitter = 'unknown'",
    });
    expect(measured.result).toBe(WARNING);
  });

  it("check_ntp_offset measures jitter across samples", async () => {
    // The server alternates its served time by +/-100ms, so successive offsets
    // differ by ~200ms regardless of how fast loopback is.
    const s = await startNtpResponder({ skewSequenceMs: [100, -100] });
    const q = await executeQuery(key, "check_ntp_offset", {
      server: "127.0.0.1",
      port: String(s.port),
      samples: "6",
      // The swing also moves the offset past its default threshold; this test
      // is about the jitter measurement, so neutralise those.
      warning: "none",
      critical: "none",
      "top-syntax": "${list}",
      "detail-syntax": "samples=${samples} jitter=${jitter}",
    });
    const m = messageOf(q).match(/samples=(\d+) jitter=(\d+)/);
    expect(m).not.toBeNull();
    expect(Number(m?.[1])).toBe(6);
    // ~200ms of swing, allowing for loopback scheduling noise.
    expect(Number(m?.[2])).toBeGreaterThan(150);
    expect(Number(m?.[2])).toBeLessThan(250);
  });

  it("check_ntp_offset separates a steady offset from jitter", async () => {
    // A clock that is consistently 5s wrong is inaccurate but perfectly
    // stable: the offset must be large and the jitter near zero, so the two
    // conditions can be alerted on independently.
    const s = await startNtpResponder({ skewSequenceMs: [5000] });
    const q = await executeQuery(key, "check_ntp_offset", {
      server: "127.0.0.1",
      port: String(s.port),
      samples: "5",
      warning: "none",
      critical: "none",
      "top-syntax": "${list}",
      "detail-syntax": "offset=${offset} jitter=${jitter}",
    });
    const m = messageOf(q).match(/offset=(\d+) jitter=(\d+)/);
    expect(m).not.toBeNull();
    expect(Number(m?.[1])).toBeGreaterThan(4000);
    expect(Number(m?.[2])).toBeLessThan(50);
  });

  it("check_ntp_offset alerts on jitter alone", async () => {
    const s = await startNtpResponder({ skewSequenceMs: [80, -80] });
    const q = await executeQuery(key, "check_ntp_offset", {
      server: "127.0.0.1",
      port: String(s.port),
      samples: "6",
      warning: "none",
      critical: "jitter > 50",
      "top-syntax": "${list}",
      "detail-syntax": "jitter=${jitter}",
    });
    expect(q.result).toBe(CRITICAL);
  });

  it("check_ntp_offset stops sampling at the first failure", async () => {
    // Nothing is listening: a burst must still cost a single timeout, not one
    // per sample, so a dead server does not multiply the check's runtime.
    const port = await closedPort();
    const started = Date.now();
    const q = await executeQuery(key, "check_ntp_offset", {
      server: "127.0.0.1",
      port: String(port),
      samples: "5",
      timeout: "700",
    });
    const elapsed = Date.now() - started;
    expect(q.result).toBe(CRITICAL);
    // One timeout (~700ms), not five (~3500ms).
    expect(elapsed).toBeLessThan(2000);
  });

  // --- address-family (ipv4 / ipv6) -----------------------------------------
  //
  // Each case binds the target to ONE loopback family and then asks the check
  // for that family and the other one. A check that ignored the flag would pass
  // both halves (the resolver would just pick the family that works), so the
  // negative half is what actually proves the transport changed.

  it("check_tcp address-family=ipv6 connects over IPv6", async () => {
    const s = await startTcpGreeter("220 service ready\r\n", "::1");
    const ok = await executeQuery(key, "check_tcp", {
      host: "localhost",
      port: String(s.port),
      "address-family": "ipv6",
      expect: "220",
    });
    expect(ok.result).toBe(OK);

    // Same name and port over IPv4: nothing is listening there.
    const bad = await executeQuery(key, "check_tcp", {
      host: "localhost",
      port: String(s.port),
      "address-family": "ipv4",
    });
    expect(bad.result).toBe(CRITICAL);
  });

  it("check_tcp address-family=ipv4 connects over IPv4", async () => {
    const s = await startTcpGreeter("220 service ready\r\n", "127.0.0.1");
    const ok = await executeQuery(key, "check_tcp", {
      host: "localhost",
      port: String(s.port),
      "address-family": "ipv4",
      expect: "220",
    });
    expect(ok.result).toBe(OK);

    const bad = await executeQuery(key, "check_tcp", {
      host: "localhost",
      port: String(s.port),
      "address-family": "ipv6",
    });
    expect(bad.result).toBe(CRITICAL);
  });

  it("check_tcp accepts the short address-family aliases", async () => {
    const s = await startTcpGreeter("220 service ready\r\n", "::1");
    for (const alias of ["6", "v6", "inet6", "IPv6"]) {
      const q = await executeQuery(key, "check_tcp", {
        host: "localhost",
        port: String(s.port),
        "address-family": alias,
        expect: "220",
      });
      expect(q.result).toBe(OK);
    }
  });

  it("check_tcp rejects an unknown address-family", async () => {
    const s = await startTcpGreeter("220 service ready\r\n");
    const q = await executeQuery(key, "check_tcp", {
      host: "127.0.0.1",
      port: String(s.port),
      "address-family": "ipv64",
    });
    expect(messageOf(q)).toMatch(/Invalid address-family: ipv64/);
  });

  it("check_ping rejects a size/count combination that would flood the target", async () => {
    // size= is caller-controlled up to 64KB and count= is unbounded, so the
    // product decides how many bytes the agent throws at an arbitrary host.
    // The guard runs before any socket is opened, so this case does not need
    // the raw-socket privileges check_ping normally requires.
    const q = await executeQuery(key, "check_ping", {
      host: "127.0.0.1",
      size: "65507",
      count: "5000",
    });
    expect(messageOf(q)).toMatch(/Refusing to send/);
    expect(messageOf(q)).toMatch(/limit is/);
  });

  it("check_ping still accepts an ordinary size and count", async () => {
    // The guard bounds bytes, not count: the same count that is refused with a
    // 64KB payload above must pass with the default ~22 byte one, so existing
    // high-count checks are unaffected.
    //
    // No host is given on purpose. The volume guard runs before the host list
    // is examined, so "No host specified" means execution got past it - and
    // this stays a check that opens no socket, like its sibling above. Actually
    // pinging would need raw-socket privileges, and 5000 sequential pings (each
    // one waiting out its own timeout when loopback ICMP is not answered, as on
    // Windows) take far longer than the suite's timeout, blocking every test
    // that follows on the shared nscp instance.
    const q = await executeQuery(key, "check_ping", {
      count: "5000",
    });
    expect(messageOf(q)).not.toMatch(/Refusing to send/);
    expect(messageOf(q)).toMatch(/No host specified/);
  });

  // --- real network, real trust store ---------------------------------------
  // Everything else in this file talks to a listener on loopback, and every TLS
  // case passes `verify: none` or an explicit `ca=`. That is deliberate - the
  // suite stays hermetic - but it means one thing is never exercised: the
  // *default* CA bundle, `${ca-path}`, which CheckNet::loadModuleEx resolves
  // once at load and hands to check_http whenever the caller does not override
  // it. ${ca-path} is a single hardcoded path (service/path_manager.cpp) and it
  // is the Debian/Ubuntu one on every non-Windows platform, so on RHEL-family -
  // where the bundle is /etc/pki/tls/certs/ca-bundle.crt - it names a file that
  // does not exist.
  //
  // That is not only a public-internet problem: make_context loads the CA
  // whenever `ca` is non-empty, before verify mode is considered, so
  // --- web/application server status pages ----------------------------------

  const APACHE_AUTO = [
    "localhost",
    "ServerVersion: Apache/2.4.58 (Unix)",
    "Total Accesses: 8341",
    "Total kBytes: 91674",
    "Uptime: 7254",
    "ReqPerSec: 1.14985",
    "BytesPerSec: 12940.7",
    "BusyWorkers: 3",
    "IdleWorkers: 47",
    "Scoreboard: __W_K_____W.....",
    "",
  ].join("\n");

  const NGINX_STUB = [
    "Active connections: 291 ",
    "server accepts handled requests",
    " 16630948 16630946 31070465 ",
    "Reading: 6 Writing: 179 Waiting: 106 ",
    "",
  ].join("\n");

  const PHPFPM_STATUS = [
    "pool:                 www",
    "process manager:      dynamic",
    "accepted conn:        4211",
    "listen queue:         0",
    "max listen queue:     11",
    "listen queue len:     511",
    "idle processes:       7",
    "active processes:     3",
    "total processes:      10",
    "max active processes: 9",
    "max children reached: 0",
    "slow requests:        5",
    "",
  ].join("\n");

  const TOMCAT_XML =
    `<?xml version="1.0" encoding="utf-8"?><status>` +
    `<jvm><memory free='1734127416' total='2147483648' max='4294967296'/></jvm>` +
    `<connector name='"http-nio-8080"'><threadInfo maxThreads="200" currentThreadCount="25" currentThreadsBusy="4"/>` +
    `<requestInfo maxTime="1230" processingTime="55211" requestCount="104211" errorCount="17" bytesReceived="0" bytesSent="1048576000"/></connector>` +
    `<connector name='"ajp-nio-8009"'><threadInfo maxThreads="100" currentThreadCount="0" currentThreadsBusy="0"/>` +
    `<requestInfo maxTime="0" processingTime="0" requestCount="0" errorCount="0" bytesReceived="0" bytesSent="0"/></connector></status>`;

  it("check_apache_status parses the ?auto page (appending ?auto itself)", async () => {
    // The handler only answers the machine-readable format when ?auto is
    // present, so an OK here proves the check appended it to the bare URL.
    const s = await startHttp((req, res) => {
      res.end(req.url?.includes("auto") ? APACHE_AUTO : "<html>Apache Status</html>");
    });
    const q = await executeQuery(key, "check_apache_status", {
      url: `http://127.0.0.1:${s.port}/server-status`,
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/3 busy and 47 idle workers/);
    expect(messageOf(q)).toMatch(/1\.14985 req\/s/);
    expect(perfValue(q, "127.0.0.1_busy_workers")).toBe(3);
  });

  it("check_apache_status applies worker thresholds", async () => {
    const s = await startHttp((_req, res) => res.end(APACHE_AUTO));
    const q = await executeQuery(key, "check_apache_status", {
      url: `http://127.0.0.1:${s.port}/server-status`,
      warning: "idle_workers < 50",
    });
    expect(q.result).toBe(WARNING);
  });

  it("check_apache_status reports parse_error for a non-status body", async () => {
    const s = await startHttp((_req, res) => res.end("<html>not a status page</html>"));
    const q = await executeQuery(key, "check_apache_status", {
      url: `http://127.0.0.1:${s.port}/server-status`,
    });
    expect(q.result).toBe(CRITICAL);
    expect(messageOf(q)).toMatch(/parse_error/);
  });

  it("check_nginx_status parses stub_status", async () => {
    const s = await startHttp((_req, res) => res.end(NGINX_STUB));
    const q = await executeQuery(key, "check_nginx_status", {
      url: `http://127.0.0.1:${s.port}/nginx_status`,
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/291 active \(6 reading, 179 writing, 106 waiting\)/);
    expect(perfValue(q, "127.0.0.1_active")).toBe(291);
  });

  it("check_nginx_status alerts on dropped connections via the derived keyword", async () => {
    // accepts=16630948, handled=16630946 -> dropped=2.
    const s = await startHttp((_req, res) => res.end(NGINX_STUB));
    const q = await executeQuery(key, "check_nginx_status", {
      url: `http://127.0.0.1:${s.port}/nginx_status`,
      warning: "dropped > 0",
    });
    expect(q.result).toBe(WARNING);
  });

  it("check_nginx_status goes CRITICAL when the endpoint is unreachable", async () => {
    const port = await closedPort();
    const q = await executeQuery(key, "check_nginx_status", {
      url: `http://127.0.0.1:${port}/nginx_status`,
    });
    expect(q.result).toBe(CRITICAL);
    expect(messageOf(q)).toMatch(/error/);
  });

  it("check_phpfpm_status parses the FPM status page", async () => {
    const s = await startHttp((_req, res) => res.end(PHPFPM_STATUS));
    const q = await executeQuery(key, "check_phpfpm_status", {
      url: `http://127.0.0.1:${s.port}/status`,
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/pool www: 3 active, 7 idle, 0 queued/);
    expect(perfValue(q, "www_active_processes")).toBe(3);
  });

  it("check_phpfpm_status applies thresholds on saturation counters", async () => {
    const s = await startHttp((_req, res) => res.end(PHPFPM_STATUS));
    const q = await executeQuery(key, "check_phpfpm_status", {
      url: `http://127.0.0.1:${s.port}/status`,
      critical: "slow_requests > 4",
    });
    expect(q.result).toBe(CRITICAL);
  });

  it("check_tomcat_status parses the manager XML with Basic auth (appending XML=true itself)", async () => {
    const s = await startHttp((req, res) => {
      const auth = "Basic " + Buffer.from("tomcat:s3cret").toString("base64");
      if (req.headers.authorization !== auth) {
        res.statusCode = 401;
        res.end("unauthorized");
        return;
      }
      res.end(req.url?.includes("XML=true") ? TOMCAT_XML : "<html/>");
    });
    const q = await executeQuery(key, "check_tomcat_status", {
      url: `http://127.0.0.1:${s.port}/manager/status`,
      username: "tomcat",
      password: "s3cret",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/http-nio-8080 ok: 4\/200 threads busy/);
    expect(messageOf(q)).toMatch(/ajp-nio-8009 ok: 0\/100 threads busy/);
    expect(perfValue(q, "http-nio-8080_threads_busy")).toBe(4);
  });

  it("check_tomcat_status goes CRITICAL without credentials", async () => {
    const s = await startHttp((_req, res) => {
      res.statusCode = 401;
      res.end("unauthorized");
    });
    const q = await executeQuery(key, "check_tomcat_status", {
      url: `http://127.0.0.1:${s.port}/manager/status`,
    });
    expect(q.result).toBe(CRITICAL);
    expect(messageOf(q)).toMatch(/http_401/);
  });

  it("check_tomcat_status applies per-connector thresholds", async () => {
    const s = await startHttp((req, res) => res.end(req.url?.includes("XML=true") ? TOMCAT_XML : "<html/>"));
    const q = await executeQuery(key, "check_tomcat_status", {
      url: `http://127.0.0.1:${s.port}/manager/status`,
      warning: "error_count > 10",
    });
    expect(q.result).toBe(WARNING);
  });

  it("check_tomcat_status reports parse_error for XML without connectors", async () => {
    // A truncated/partial manager response can parse as XML yet carry zero
    // <connector> elements; that must not be reported as "OK: 0/0 threads".
    const jvmOnly = `<?xml version="1.0" encoding="utf-8"?><status><jvm><memory free='1' total='2' max='3'/></jvm></status>`;
    const s = await startHttp((_req, res) => res.end(jvmOnly));
    const q = await executeQuery(key, "check_tomcat_status", {
      url: `http://127.0.0.1:${s.port}/manager/status`,
    });
    expect(q.result).toBe(CRITICAL);
    expect(messageOf(q)).toMatch(/parse_error/);
  });

  it("status-page checks report invalid_url for a malformed port instead of throwing", async () => {
    // ":" with nothing after it used to hit an unguarded std::stoll and escape
    // the check as a generic module exception; ":8o80" silently parsed as 8.
    for (const url of ["http://127.0.0.1:/server-status", "http://127.0.0.1:8o80/server-status"]) {
      const q = await executeQuery(key, "check_apache_status", { url });
      expect(q.result).toBe(CRITICAL);
      expect(messageOf(q)).toMatch(/invalid_url/);
    }
  });

  it("status-page checks reject a non-positive timeout", async () => {
    // timeout=-1 used to be cast to unsigned and become a ~136-year timeout.
    const q = await executeQuery(key, "check_nginx_status", {
      url: "http://127.0.0.1/nginx_status",
      timeout: "-1",
    });
    expect(q.result).toBe(UNKNOWN);
    expect(messageOf(q)).toMatch(/timeout must be a positive number of seconds/);
  });

  // `verify=none` against a local self-signed server fails too. On Rocky 10 it
  // takes down four of this file's existing tests (ssl_expiry_days, the
  // redirect-to-plain-http case and both check_nsclient_web_online cases) with
  // "Failed to load CA /etc/ssl/certs/ca-certificates.crt". They pass on Debian,
  // which is the only place this suite used to run.
  //
  // This is the only test here that needs egress, and it asserts reachability
  // rather than latency, so a slow or busy runner cannot turn a working check
  // into a red build.
  describe("against the public internet", () => {
    it("check_http validates a public HTTPS site using the platform CA bundle", async () => {
      const q = await executeQuery(key, "check_http", {
        url: "https://www.google.com",
        // No ca= and no verify=none on purpose: the point is the default.
        critical: "code != 200",
      });
      // Assert the CA failure separately from the result, so a broken trust
      // store reads as itself instead of as a generic CRITICAL.
      expect(messageOf(q)).not.toMatch(/Failed to load CA/);
      expect(q.result).toBe(OK);
    });
  });

  it("check_ssh honours address-family", async () => {
    const s = await startTcpGreeter("SSH-2.0-OpenSSH_9.6p1\r\n", "::1");
    const q = await executeQuery(key, "check_ssh", {
      host: "localhost",
      port: String(s.port),
      "address-family": "ipv6",
      "top-syntax": "${list}",
      "detail-syntax": "${software} ${software_version}",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toBe("OpenSSH 9.6p1");
  });

  it("check_http address-family=ipv6 fetches over IPv6", async () => {
    const s = await startHttp((_req, res) => {
      res.writeHead(200, { "Content-Type": "text/plain" });
      res.end("ok");
    }, undefined, "::1");
    const ok = await executeQuery(key, "check_http", {
      url: `http://localhost:${s.port}/`,
      "address-family": "ipv6",
    });
    expect(ok.result).toBe(OK);

    const bad = await executeQuery(key, "check_http", {
      url: `http://localhost:${s.port}/`,
      "address-family": "ipv4",
    });
    expect(bad.result).toBe(CRITICAL);
  });

  it("check_http accepts a bracketed IPv6 literal URL", async () => {
    // The host is echoed back so the Host header can be asserted: RFC 7230
    // wants the brackets kept there even though the resolver needs them gone.
    const s = await startHttp((req, res) => {
      const body = `host=${req.headers.host}`;
      res.writeHead(200, { "Content-Type": "text/plain" });
      res.end(body);
    }, undefined, "::1");
    const q = await executeQuery(key, "check_http", {
      url: `http://[::1]:${s.port}/`,
      "top-syntax": "${list}",
      "detail-syntax": "${host}|${port}|${code}|${body}",
    });
    expect(q.result).toBe(OK);
    // Brackets are kept in the Host header. (The port is not sent there for any
    // address family - a separate, pre-existing gap, not an IPv6 one.)
    expect(messageOf(q)).toBe(`::1|${s.port}|200|host=[::1]`);
  });

  it("check_dns queries an IPv6 DNS server", async () => {
    // The DNS server itself is reached over IPv6; the record it returns is an
    // ordinary A record, which keeps the two concepts (transport vs record
    // type) visibly separate.
    const s = await startDnsResponder([10, 1, 2, 3], "::1");
    const q = await executeQuery(key, "check_dns", {
      host: "example.com",
      server: "::1",
      port: String(s.port),
      "address-family": "ipv6",
      "top-syntax": "${list}",
      "detail-syntax": "${addresses}",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toBe("10.1.2.3");
  });

  it("check_ntp_offset queries an IPv6 NTP server", async () => {
    const s = await startNtpResponder({ bind: "::1" });
    const q = await executeQuery(key, "check_ntp_offset", {
      server: "::1",
      port: String(s.port),
      "address-family": "ipv6",
      warning: "stratum >= 16",
      critical: "result != 'ok'",
      "top-syntax": "${list}",
      "detail-syntax": "${result} stratum=${stratum}",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toBe("ok stratum=2");
  });

  // --- check_nsclient_web_online -------------------------------------------

  it("check_nsclient_web_online reports a reachable REST API", async () => {
    const s = await startHttp((req, res) => {
      res.writeHead(200, { "Content-Type": "application/json" });
      // /api/v1/info reachability probe.
      res.end(JSON.stringify({ version: "test", name: "mock" }));
    }, serverCert);
    const q = await executeQuery(key, "check_nsclient_web_online", {
      url: `https://127.0.0.1:${s.port}`,
      password: "irrelevant",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/reachable/i);
  });

  it("check_nsclient_web_online passes a remote check result through", async () => {
    const s = await startHttp((req, res) => {
      res.writeHead(200, { "Content-Type": "application/json" });
      if (req.url && req.url.includes("/commands/execute")) {
        // Remote result: nagios code 0 (OK) with a message.
        res.end(JSON.stringify({ result: 0, lines: [{ message: "remote all good" }] }));
      } else {
        res.end(JSON.stringify({ version: "test" }));
      }
    }, serverCert);
    const q = await executeQuery(key, "check_nsclient_web_online", {
      url: `https://127.0.0.1:${s.port}`,
      password: "irrelevant",
      command: "check_ok",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/remote all good/);
  });
});
