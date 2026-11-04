/**
 * Two things about how the WEB server treats a connection, rather than what
 * it answers on it.
 *
 * Keep-alive: the beast backend used to answer every request with
 * `Connection: close`, so the web UI and a monitoring server polling the REST
 * API paid a TCP and TLS handshake per request. A client that keeps its
 * connection now gets to reuse it on either backend.
 *
 * `max request size`: the cap on a request body, which was a compile-time
 * constant of each backend (1 MiB on beast, 3 MiB on mongoose) that dropped
 * the connection without a word. It is a setting now, and a request over it
 * is answered 413 and logged.
 */
import https from "node:https";
import tls from "node:tls";
import { NscpInstance, setupRestNscp } from "@fixtures/index";

jest.setTimeout(900_000);

const WEB_PORT = 8443;

type Answer = { status: number; body: string; reusedSocket: boolean };

/**
 * One request through `agent`, so the test controls whether the socket is
 * kept. supertest hides the agent and the socket from us.
 */
function send(agent: https.Agent, method: string, path: string, body?: Buffer): Promise<Answer> {
  return new Promise((resolve, reject) => {
    const req = https.request(
      {
        host: "127.0.0.1",
        port: WEB_PORT,
        path,
        method,
        agent,
        auth: "admin:default-password",
        headers: body
          ? { "content-type": "application/octet-stream", "content-length": body.length }
          : {},
        rejectUnauthorized: false,
      },
      (res) => {
        const chunks: Buffer[] = [];
        res.on("data", (chunk: Buffer) => chunks.push(chunk));
        res.on("end", () =>
          resolve({
            status: res.statusCode ?? 0,
            body: Buffer.concat(chunks).toString("utf8"),
            reusedSocket: req.reusedSocket,
          }),
        );
        res.on("error", reject);
      },
    );
    req.on("error", reject);
    if (body) req.write(body);
    req.end();
  });
}

const BASIC_AUTH = `Authorization: Basic ${Buffer.from("admin:default-password").toString("base64")}\r\n`;

/**
 * One request written straight onto a TLS socket, and whether the server
 * hung up afterwards: `closedWithin` is true when the socket ended within a
 * second of the answer, false when it was still open (and the test closes
 * it). The answer is complete once Content-Length bytes of body have come.
 */
function rawRequest(
  request: string,
  within = 1_000,
): Promise<{ status: number; closedWithin: boolean }> {
  return new Promise((resolve, reject) => {
    const socket = tls.connect(
      { host: "127.0.0.1", port: WEB_PORT, rejectUnauthorized: false },
      () => socket.write(request),
    );
    let received = Buffer.alloc(0);
    let status = 0;
    let settled = false;
    let timer: NodeJS.Timeout | undefined;
    const settle = (closedWithin: boolean) => {
      if (settled) return;
      settled = true;
      if (timer) clearTimeout(timer);
      socket.destroy();
      resolve({ status, closedWithin });
    };
    socket.on("data", (chunk: Buffer) => {
      received = Buffer.concat([received, chunk]);
      if (timer) return;
      const headerEnd = received.indexOf("\r\n\r\n");
      if (headerEnd < 0) return;
      const head = received.subarray(0, headerEnd).toString("latin1");
      status = Number(/^HTTP\/1\.[01] (\d{3})/.exec(head)?.[1] ?? 0);
      const length = Number(/^content-length:\s*(\d+)/im.exec(head)?.[1] ?? 0);
      if (received.length - headerEnd - 4 >= length)
        timer = setTimeout(() => settle(false), within);
    });
    socket.on("end", () => settle(true));
    socket.on("close", () => settle(true));
    socket.on("error", (error) => (settled ? undefined : reject(error)));
    setTimeout(
      () => (settled ? undefined : reject(new Error("no answer within 10 s"))),
      10_000,
    ).unref();
  });
}

describe("WEB server connections", () => {
  let nscp: NscpInstance;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await setupRestNscp(nscp);
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("keeps a connection alive for the client's next request", async () => {
    const agent = new https.Agent({ keepAlive: true, maxSockets: 1 });
    try {
      const first = await send(agent, "GET", "/api/v2/info");
      expect(first.status).toBe(200);
      // Node reports whether the socket came out of the agent's free pool,
      // which it only can when the server left it open after the first answer.
      const second = await send(agent, "GET", "/api/v2/info");
      expect(second.status).toBe(200);
      expect(second.reusedSocket).toBe(true);
      const third = await send(agent, "GET", "/api/v2/info");
      expect(third.status).toBe(200);
      expect(third.reusedSocket).toBe(true);
    } finally {
      agent.destroy();
    }
  });

  it("answers HEAD without a body, so the next answer on the connection is readable", async () => {
    // A client knows a HEAD answer has no body and reads the next status
    // line right after the header; a server that sends the body anyway
    // leaves it in the socket as the start of the next answer.
    const agent = new https.Agent({ keepAlive: true, maxSockets: 1 });
    try {
      const head = await send(agent, "HEAD", "/api/v2/info");
      expect(head.body).toBe("");
      const next = await send(agent, "GET", "/api/v2/info");
      expect(next.status).toBe(200);
      expect(next.reusedSocket).toBe(true);
      expect(JSON.parse(next.body)).toHaveProperty("name");
    } finally {
      agent.destroy();
    }
  });

  it("leaves the connection open unless the client asks it to close", async () => {
    // Seen from the socket rather than from a header: the mongoose backend
    // closes without saying `Connection: close`, which HTTP allows, so what
    // both backends have in common is what happens to the connection.
    const kept = await rawRequest(
      "GET /api/v2/info HTTP/1.1\r\nHost: 127.0.0.1\r\n" + BASIC_AUTH + "\r\n",
    );
    expect(kept.status).toBe(200);
    expect(kept.closedWithin).toBe(false);
    const closed = await rawRequest(
      "GET /api/v2/info HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n" + BASIC_AUTH + "\r\n",
    );
    expect(closed.status).toBe(200);
    expect(closed.closedWithin).toBe(true);
  });

  it("accepts a request body up to the default max request size", async () => {
    // 2 MiB: over beast's old built-in 1 MiB, under the 3 MiB default. The
    // route does not take a POST, so anything but a 413 means the body got
    // through the HTTP layer, which is what this is about.
    const agent = new https.Agent({ keepAlive: false });
    try {
      const answer = await send(agent, "POST", "/api/v2/info", Buffer.alloc(2 * 1024 * 1024, 0x78));
      expect(answer.status).not.toBe(413);
    } finally {
      agent.destroy();
    }
  });
});

describe("WEB server with max request size = 4k", () => {
  let nscp: NscpInstance;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": { WEBServer: "enabled" },
      "/settings/default": { "allowed hosts": "127.0.0.1,::1" },
      "/settings/WEB/server": { "max request size": "4k" },
      "/settings/WEB/server/users/admin": { role: "full", password: "default-password" },
    });
    await nscp.waitForPortFree(WEB_PORT, { timeoutMs: 30_000 });
    nscp.start();
    await nscp.waitForPort(WEB_PORT, { timeoutMs: 30_000 });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("answers a request over the cap with 413 and logs it", async () => {
    const agent = new https.Agent({ keepAlive: false });
    try {
      const small = await send(agent, "POST", "/api/v2/info", Buffer.alloc(1024, 0x78));
      expect(small.status).not.toBe(413);
      const big = await send(agent, "POST", "/api/v2/info", Buffer.alloc(8 * 1024, 0x78));
      expect(big.status).toBe(413);
      expect(big.body).toContain("max request size");
      expect(big.body).toContain("4096");
    } finally {
      agent.destroy();
    }
    const deadline = Date.now() + 10_000;
    while (
      Date.now() < deadline &&
      !nscp.capturedStdout().includes("Rejected a request from 127.0.0.1")
    ) {
      await new Promise((r) => setTimeout(r, 100));
    }
    expect(nscp.capturedStdout()).toContain(
      "Rejected a request from 127.0.0.1: its body is larger than the 4096 bytes 'max request size' allows",
    );
  });
});

describe("WEB server with a max request size it cannot read", () => {
  let nscp: NscpInstance;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": { WEBServer: "enabled" },
      "/settings/default": { "allowed hosts": "127.0.0.1,::1" },
      "/settings/WEB/server": { "max request size": "lots" },
      "/settings/WEB/server/users/admin": { role: "full", password: "default-password" },
    });
    await nscp.waitForPortFree(WEB_PORT, { timeoutMs: 30_000 });
    nscp.start();
    await nscp.waitForPort(WEB_PORT, { timeoutMs: 30_000 });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("says so and serves with the default", async () => {
    expect(nscp.capturedStdout()).toContain(
      "Ignoring 'max request size = lots': expected a number of bytes",
    );
    const agent = new https.Agent({ keepAlive: false });
    try {
      const answer = await send(agent, "GET", "/api/v2/info");
      expect(answer.status).toBe(200);
      const over_a_mebibyte = await send(
        agent,
        "POST",
        "/api/v2/info",
        Buffer.alloc(2 * 1024 * 1024, 0x78),
      );
      expect(over_a_mebibyte.status).not.toBe(413);
    } finally {
      agent.destroy();
    }
  });
});
