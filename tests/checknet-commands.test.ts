/**
 * Exercises the CheckNet module's recently-added network checks end-to-end
 * against throwaway local servers (net / tls / http / https / dgram), so the
 * suite is fully self-contained and needs no external network:
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
  type CertPair,
  executeQuery,
  generateCertChain,
  messageOf,
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

/** A plain-TCP server that greets each client and closes (FTP/SMTP style). */
function startTcpGreeter(greeting: string): Promise<Listener> {
  return new Promise((resolve) => {
    const srv = net.createServer((sock) => {
      sock.on("error", () => {});
      sock.end(greeting); // write banner then FIN → the check sees a clean EOF
    });
    srv.listen(0, "127.0.0.1", () =>
      resolve(track({ port: portOf(srv), close: closeNetServer(srv) })),
    );
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
function startHttp(handler: http.RequestListener, cert?: CertPair): Promise<Listener> {
  return new Promise((resolve) => {
    const srv = cert
      ? https.createServer({ key: cert.keyPem, cert: cert.certPem }, handler)
      : http.createServer(handler);
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

/**
 * A minimal UDP DNS responder: echoes the query's transaction id + question and
 * answers every query with a single A record for the given IPv4 address. Enough
 * to exercise check_dns's DNS-over-UDP client and A-record parser end to end.
 */
function startDnsResponder(
  ip: [number, number, number, number] = [93, 184, 216, 34],
): Promise<Listener> {
  return new Promise((resolve) => {
    const sock = dgram.createSocket("udp4");
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
    sock.bind(0, "127.0.0.1", () =>
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
