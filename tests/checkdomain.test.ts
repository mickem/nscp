/** Domain registration checks against local HTTPS and WHOIS fixtures.
 * One-shot client queries exercise command dispatch and REST-style k=v tokens
 * without depending on WEBServer or public registration services.
 */
import * as http from "node:http";
import * as https from "node:https";
import * as net from "node:net";
import * as tls from "node:tls";
import {
  CRITICAL,
  NscpInstance,
  OK,
  UNKNOWN,
  WARNING,
  type CertPair,
  generateCertChain,
} from "@fixtures/index";

jest.setTimeout(30_000);

interface Listener {
  port: number;
  close: () => Promise<void>;
}
const openListeners: Listener[] = [];
function track(listener: Listener): Listener {
  openListeners.push(listener);
  return listener;
}
function portOf(server: net.Server): number {
  return (server.address() as net.AddressInfo).port;
}
function closeNetServer(server: net.Server): () => Promise<void> {
  return () => new Promise<void>((resolve) => server.close(() => resolve()));
}
function startHttp(handler: http.RequestListener, cert: CertPair): Promise<Listener> {
  return new Promise((resolve) => {
    const server = https.createServer({ key: cert.keyPem, cert: cert.certPem }, handler);
    server.listen(0, "127.0.0.1", () =>
      resolve(track({ port: portOf(server), close: closeNetServer(server) })),
    );
  });
}
function startTcpGreeter(text: string): Promise<Listener> {
  return new Promise((resolve) => {
    const server = net.createServer((socket) => socket.once("data", () => socket.end(text)));
    server.listen(0, "127.0.0.1", () =>
      resolve(track({ port: portOf(server), close: closeNetServer(server) })),
    );
  });
}

describe("CheckNet check_domain", () => {
  let nscp: NscpInstance;
  let serverCert: CertPair;
  let caCert: CertPair;
  beforeAll(() => {
    nscp = new NscpInstance();
    const bundle = generateCertChain({
      outDir: nscp.scratch("domain_certs"),
      signed: { server: { commonName: "localhost", isServer: true } },
    });
    serverCert = bundle.signed.server;
    caCert = bundle.ca;
  });
  afterEach(async () => {
    await Promise.all(openListeners.splice(0).map((listener) => listener.close()));
  });
  async function query(args: Record<string, string>) {
    const result = await nscp.run(
      [
        "client",
        "--module",
        "CheckNet",
        "--boot",
        "--query",
        "check_domain",
        ...Object.entries(args).map(([key, value]) => `${key}=${value}`),
      ],
      { allowFailure: true },
    );
    return { result: result.exitCode, output: result.all ?? result.stdout };
  }
  // --- check_domain ---------------------------------------------------------

  function domainRecord(days: number, extra: object = {}) {
    return {
      objectClassName: "domain",
      ldhName: "example.com",
      events: [
        {
          eventAction: "expiration",
          eventDate: new Date(Date.now() + (days * 86400 + 3600) * 1000).toISOString(),
        },
      ],
      ...extra,
    };
  }

  async function domainQuery(handler: http.RequestListener, args: Record<string, string> = {}) {
    const s = await startHttp(handler, serverCert);
    return query({
      domain: "example.com",
      "rdap-url": `https://127.0.0.1:${s.port}/domain/{domain}`,
      ca: caCert.certPath,
      ...args,
    });
  }

  async function rawDomainQuery(
    response: string,
    keepOpen: boolean,
    args: Record<string, string> = {},
  ) {
    const sockets = new Set<tls.TLSSocket>();
    const server = tls.createServer(
      { key: serverCert.keyPem, cert: serverCert.certPem },
      (socket) => {
        sockets.add(socket);
        socket.on("close", () => sockets.delete(socket));
        socket.on("error", () => {});
        socket.once("data", () => {
          if (keepOpen) socket.write(response);
          else socket.end(response);
        });
      },
    );
    await new Promise<void>((resolve) => server.listen(0, "127.0.0.1", resolve));
    track({
      port: portOf(server),
      close: async () => {
        for (const socket of sockets) socket.destroy();
        await closeNetServer(server)();
      },
    });
    return query({
      domain: "example.com",
      "rdap-url": `https://127.0.0.1:${portOf(server)}/domain/example.com`,
      ca: caCert.certPath,
      timeout: "1",
      ...args,
    });
  }

  it.each([
    "length-timeout",
    "length-eof",
    "chunk-timeout",
    "chunk-eof",
    "trailer-eof",
    "close-timeout",
  ])("check_domain rejects valid JSON in an incomplete HTTP response: %s", async (mode) => {
    const body = JSON.stringify(domainRecord(100));
    let framing: string;
    if (mode.startsWith("length"))
      framing = `Content-Length: ${Buffer.byteLength(body) + 20}\r\n\r\n${body}`;
    else if (mode.startsWith("chunk") || mode === "trailer-eof") {
      framing = `Transfer-Encoding: chunked\r\n\r\n${Buffer.byteLength(body).toString(16)}\r\n${body}\r\n`;
      if (mode === "trailer-eof") framing += "0\r\nX-Trailer: incomplete\r\n";
    } else framing = `Connection: close\r\n\r\n${body}`;
    const q = await rawDomainQuery(`HTTP/1.1 200 OK\r\n${framing}`, mode.endsWith("timeout"));
    expect(q.result).toBe(UNKNOWN);
    expect(q.output).toMatch(/timed out|Incomplete HTTP response/i);
    expect(q.output).not.toMatch(/\|.*=/);
  });

  it.each(["length", "chunked"])(
    "check_domain finishes a complete %s response without waiting for socket closure",
    async (mode) => {
      const body = JSON.stringify(domainRecord(100));
      const framing =
        mode === "length"
          ? `Content-Length: ${Buffer.byteLength(body)}\r\n\r\n${body}`
          : `Transfer-Encoding: chunked\r\n\r\n${Buffer.byteLength(body).toString(16)}\r\n${body}\r\n0\r\n\r\n`;
      const started = Date.now();
      const q = await rawDomainQuery(`HTTP/1.1 200 OK\r\n${framing}`, true, { timeout: "3" });
      expect(q.result).toBe(OK);
      expect(q.output).toContain("expires in 100d");
      expect(Date.now() - started).toBeLessThan(2500);
    },
  );

  it("check_domain falls back to WHOIS on an incomplete RDAP transfer", async () => {
    const body = JSON.stringify(domainRecord(100));
    const whois = await startTcpGreeter(
      `Registry Expiry Date: ${domainRecord(20).events[0].eventDate}\r\n`,
    );
    const q = await rawDomainQuery(
      `HTTP/1.1 200 OK\r\nContent-Length: ${Buffer.byteLength(body) + 20}\r\n\r\n${body}`,
      true,
      {
        "whois-fallback": "true",
        "whois-server": "127.0.0.1",
        "whois-port": String(whois.port),
      },
    );
    expect(q.result).toBe(WARNING);
    expect(q.output).toContain("expires in 20d");
    expect(q.output).toContain("whois:127.0.0.1:");
  });

  it("check_domain normalizes parent paths in redirects", async () => {
    const paths: string[] = [];
    const q = await domainQuery((req, res) => {
      paths.push(req.url ?? "");
      if (req.url === "/domain/example.com") {
        res.writeHead(302, { Location: "/nested/redirect" });
        res.end();
      } else if (req.url === "/nested/redirect") {
        res.writeHead(302, { Location: "../final?value=../preserved" });
        res.end();
      } else if (req.url === "/final?value=../preserved")
        res.end(JSON.stringify(domainRecord(100)));
      else {
        res.writeHead(404);
        res.end();
      }
    });
    expect(q.result).toBe(OK);
    expect(paths).toEqual(["/domain/example.com", "/nested/redirect", "/final?value=../preserved"]);
  });

  it.each([
    [100, OK],
    [20, WARNING],
    [5, CRITICAL],
    [-2, CRITICAL],
  ])("check_domain maps %i days to the expected state", async (days, state) => {
    const q = await domainQuery(
      (req, res) => {
        expect(req.url).toBe("/domain/example.com");
        res.end(JSON.stringify(domainRecord(days)));
      },
      { domain: "EXAMPLE.COM." },
    );
    expect(q.result).toBe(state);
    expect(q.output).toContain(`example.com expires in ${days}d`);
    expect(q.output).toContain("registry, rdap:https://127.0.0.1:");
    expect(q.output).toContain("'example.com'=" + days + "d;");
  });

  it("check_domain uses HTTP/1.1 for both bootstrap and authoritative requests", async () => {
    const paths: string[] = [];
    const q = await domainQuery((req, res) => {
      paths.push(req.url ?? "");
      if (req.httpVersion !== "1.1") {
        res.writeHead(505);
        res.end();
      } else if (req.url === "/domain/example.com") {
        res.writeHead(302, { Location: "/authoritative" });
        res.end();
      } else {
        res.end(JSON.stringify(domainRecord(100)));
      }
    });
    expect(q.result).toBe(OK);
    expect(q.output).toContain("expires in 100d");
    expect(paths).toEqual(["/domain/example.com", "/authoritative"]);
  });

  it("check_domain uses custom filters and registrar expiration", async () => {
    const q = await domainQuery(
      (_req, res) =>
        res.end(
          JSON.stringify(
            domainRecord(400, {
              events: [
                ...domainRecord(400).events,
                {
                  ...domainRecord(40).events[0],
                  eventAction: "registrar expiration",
                },
              ],
            }),
          ),
        ),
      { warning: "expires_in < 50", critical: "expires_in < 5" },
    );
    expect(q.result).toBe(WARNING);
    expect(q.output).toContain("expires in 40d");
    expect(q.output).toContain("registrar,");
  });

  it("check_domain follows redirects and the related registrar domain record", async () => {
    const q = await domainQuery((req, res) => {
      if (req.url === "/domain/example.com") {
        res.writeHead(307, { Location: "/registry" });
        res.end();
      } else if (req.url === "/registry") {
        res.end(
          JSON.stringify(
            domainRecord(400, {
              links: [
                {
                  rel: "related",
                  type: "application/rdap+json",
                  href: `https://${req.headers.host}/registrar`,
                },
              ],
            }),
          ),
        );
      } else {
        res.end(
          JSON.stringify(
            domainRecord(5, {
              events: [
                {
                  ...domainRecord(5).events[0],
                  eventAction: "registrar expiration",
                },
              ],
            }),
          ),
        );
      }
    });
    expect(q.result).toBe(CRITICAL);
    expect(q.output).toContain("expires in 5d");
    expect(q.output).toContain("/registrar");
  });

  it.each([
    ["missing expiration", JSON.stringify(domainRecord(100, { events: [] }))],
    ["wrong domain", JSON.stringify(domainRecord(100, { ldhName: "other.com" }))],
    ["bad JSON", "not json"],
  ])("check_domain returns UNKNOWN for %s", async (_label, body) => {
    const q = await domainQuery((_req, res) => res.end(body));
    expect(q.result).toBe(UNKNOWN);
    expect(q.output).toContain("Domain lookup failed");
    expect(q.output).not.toMatch(/\|.*=/);
  });

  it.each([404, 429, 503])("check_domain returns UNKNOWN for HTTP %i", async (code) => {
    const q = await domainQuery((_req, res) => {
      res.writeHead(code);
      res.end();
    });
    expect(q.result).toBe(UNKNOWN);
    expect(q.output).toContain(`HTTP ${code}`);
  });

  it("check_domain rejects untrusted TLS certificates", async () => {
    const q = await domainQuery((_req, res) => res.end(JSON.stringify(domainRecord(100))), {
      ca: "",
    });
    expect(q.result).toBe(UNKNOWN);
    expect(q.output).toMatch(/certificate|verify|trust/i);
  });

  it("check_domain rejects an HTTPS to HTTP downgrade", async () => {
    const q = await domainQuery((_req, res) => {
      res.writeHead(302, { Location: "http://127.0.0.1/domain/example.com" });
      res.end();
    });
    expect(q.result).toBe(UNKNOWN);
    expect(q.output).toContain("HTTPS URL");
  });

  it("check_domain bounds redirect loops", async () => {
    const q = await domainQuery((_req, res) => {
      res.writeHead(302, { Location: "/loop" });
      res.end();
    });
    expect(q.result).toBe(UNKNOWN);
    expect(q.output).toContain("redirect limit");
  });

  it("check_domain bounds the response size", async () => {
    const q = await domainQuery((_req, res) => res.end(" ".repeat(1024 * 1024 + 1)));
    expect(q.result).toBe(UNKNOWN);
    expect(q.output).toMatch(/maximum size/i);
  });

  it("check_domain times out a server that never responds", async () => {
    const started = Date.now();
    let reached = false;
    const q = await domainQuery(
      () => {
        reached = true;
      },
      { timeout: "1" },
    );
    expect(reached).toBe(true);
    expect(q.result).toBe(UNKNOWN);
    expect(q.output).toMatch(/timeout|timed out/i);
    expect(Date.now() - started).toBeLessThan(8000);
  });

  it("check_domain accepts valued WHOIS fallback and reads the configured TCP server", async () => {
    const whois = net.createServer((socket) =>
      socket.once("data", (data) => {
        expect(data.toString()).toBe("example.com\r\n");
        socket.end(
          `Domain Name: example.com\r\nRegistry Expiry Date: ${domainRecord(100).events[0].eventDate}\r\n`,
        );
      }),
    );
    await new Promise<void>((resolve) => whois.listen(0, "127.0.0.1", resolve));
    track({ port: portOf(whois), close: closeNetServer(whois) });
    const q = await domainQuery(
      (_req, res) => {
        res.writeHead(404);
        res.end();
      },
      {
        "whois-fallback": "true",
        "whois-server": "127.0.0.1",
        "whois-port": String(portOf(whois)),
      },
    );
    expect(q.result).toBe(OK);
    expect(q.output).toContain("expires in 100d");
    expect(q.output).toContain("whois:127.0.0.1:");
  });

  it("check_domain rejects a WHOIS read timeout even after receiving an expiration", async () => {
    const started = Date.now();
    let reached = false;
    const whois = net.createServer((socket) =>
      socket.once("data", () => {
        reached = true;
        socket.write(`Registry Expiry Date: ${domainRecord(100).events[0].eventDate}\r\n`);
      }),
    );
    await new Promise<void>((resolve) => whois.listen(0, "127.0.0.1", resolve));
    track({ port: portOf(whois), close: closeNetServer(whois) });
    const q = await domainQuery(
      (_req, res) => {
        res.writeHead(404);
        res.end();
      },
      {
        timeout: "1",
        "whois-fallback": "true",
        "whois-server": "127.0.0.1",
        "whois-port": String(portOf(whois)),
      },
    );
    expect(reached).toBe(true);
    expect(q.result).toBe(UNKNOWN);
    // Socket error text is localized by the OS; assert the fallback failed
    // within the timeout instead of depending on an English error message.
    expect(q.output).toContain("WHOIS:");
    expect(q.output).not.toMatch(/\|.*=/);
    expect(Date.now() - started).toBeLessThan(8000);
  });

  it("check_domain leaves WHOIS disabled with whois-fallback=false", async () => {
    const q = await domainQuery(
      (_req, res) => {
        res.writeHead(404);
        res.end();
      },
      { "whois-fallback": "false" },
    );
    expect(q.result).toBe(UNKNOWN);
    expect(q.output).toContain("HTTP 404");
  });

  it.each([
    [{ domain: "https://example.com" }, /domain name/i],
    [{ timeout: "0" }, /timeout must be/i],
    [{ "whois-fallback": "true" }, /whois-server/i],
  ])("check_domain validates options before lookup: %j", async (args, message) => {
    const q = await query({
      domain: "example.com",
      ...args,
    });
    expect(q.result).toBe(UNKNOWN);
    expect(q.output).toMatch(message);
  });

  it("check_domain allows a response longer than timeout while reads keep progressing", async () => {
    const started = Date.now();
    let chunks = 0;
    const q = await domainQuery(
      (_req, res) => {
        res.writeHead(200, { "Content-Type": "application/rdap+json" });
        res.flushHeaders();
        const timer = setInterval(() => {
          res.write(" ");
          chunks += 1;
          if (chunks === 8) {
            clearInterval(timer);
            res.end(JSON.stringify(domainRecord(100)));
          }
        }, 200);
        res.on("close", () => clearInterval(timer));
      },
      { timeout: "1" },
    );
    expect(chunks).toBe(8);
    expect(q.result).toBe(OK);
    expect(q.output).toContain("expires in 100d");
    expect(Date.now() - started).toBeGreaterThan(1000);
    expect(Date.now() - started).toBeLessThan(8000);
  });

  it("check_domain reports an unavailable registrar instead of masking it with the registry date", async () => {
    const q = await domainQuery((req, res) => {
      if (req.url === "/registrar") {
        res.writeHead(503);
        res.end();
      } else
        res.end(
          JSON.stringify(
            domainRecord(400, {
              links: [
                {
                  rel: "related",
                  type: "application/rdap+json",
                  href: `https://${req.headers.host}/registrar`,
                },
              ],
            }),
          ),
        );
    });
    expect(q.result).toBe(UNKNOWN);
    expect(q.output).toContain("HTTP 503");
  });

  it("check_domain returns UNKNOWN with both errors when WHOIS also fails", async () => {
    const whois = await startTcpGreeter("No supported data\r\n");
    const q = await domainQuery(
      (_req, res) => {
        res.writeHead(429);
        res.end();
      },
      {
        "whois-fallback": "true",
        "whois-server": "127.0.0.1",
        "whois-port": String(whois.port),
      },
    );
    expect(q.result).toBe(UNKNOWN);
    expect(q.output).toContain("HTTP 429");
    expect(q.output).toContain("WHOIS:");
  });

  it("check_domain does not use WHOIS when RDAP succeeds", async () => {
    const q = await domainQuery((_req, res) => res.end(JSON.stringify(domainRecord(100))), {
      "whois-fallback": "true",
      "whois-server": "127.0.0.1",
      "whois-port": "1",
    });
    expect(q.result).toBe(OK);
    expect(q.output).toContain("rdap:");
  });
});
