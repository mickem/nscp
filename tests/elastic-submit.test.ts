/**
 * ElasticClient integration: points the module at a loopback HTTP server that
 * speaks just enough of the Elasticsearch _bulk API to capture what the agent
 * sends, then verifies the wire contract:
 *
 *   - metrics collected by CheckSystem are POSTed to the configured address as
 *     x-ndjson bulk requests (action line + document line);
 *   - the configured user/password arrive as a basic Authorization header;
 *   - action lines carry a per-document _id and NO legacy _type parameter
 *     (mapping types were removed in Elasticsearch 8, which rejects them);
 *   - the %(date) placeholder in the index name is expanded.
 *
 * The server is in-process (no docker): the module talks plain HTTP/1.0 with
 * Connection: close, which node's http server handles fine.
 */
import * as http from "http";
import type { AddressInfo } from "net";
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(600_000);

const USER = "elastic-user";
const PASSWORD = "s3cret-password";

interface CapturedRequest {
  url: string;
  authorization?: string;
  contentType?: string;
  body: string;
}

describe("Elastic integration", () => {
  let nscp: NscpInstance;
  let server: http.Server;
  const requests: CapturedRequest[] = [];

  async function waitForBulk(
    predicate: (r: CapturedRequest) => boolean,
    timeoutMs = 60_000,
  ): Promise<CapturedRequest | undefined> {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      const hit = requests.find(predicate);
      if (hit) return hit;
      await new Promise((r) => setTimeout(r, 500));
    }
    return undefined;
  }

  beforeAll(async () => {
    server = http.createServer((req, res) => {
      let body = "";
      req.on("data", (chunk) => (body += chunk));
      req.on("end", () => {
        requests.push({
          url: req.url ?? "",
          authorization: req.headers.authorization,
          contentType: req.headers["content-type"],
          body,
        });
        res.setHeader("Content-Type", "application/json");
        res.end(JSON.stringify({ took: 1, errors: false, items: [] }));
      });
    });
    await new Promise<void>((resolve) => server.listen(0, "127.0.0.1", () => resolve()));
    const port = (server.address() as AddressInfo).port;

    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        CheckSystem: "enabled", // metrics producer (system.cpu.* / system.mem.*)
        ElasticClient: "enabled",
      },
      "/settings/core": {
        // Push metrics frequently so the test doesn't wait the 10s default.
        "metrics interval": "1s",
      },
      "/settings/elastic/client": {
        address: `http://127.0.0.1:${port}/_bulk`,
        user: USER,
        password: PASSWORD,
      },
    });

    nscp.start();
  });

  afterAll(async () => {
    await nscp?.stop();
    await new Promise<void>((resolve) => server.close(() => resolve()));
  });

  it("submits metrics as authenticated x-ndjson bulk requests", async () => {
    const metrics = await waitForBulk((r) => r.body.includes("nsclient_metrics-"));
    expect(metrics).toBeDefined();
    if (!metrics) return;

    expect(metrics.url).toBe("/_bulk");
    expect(metrics.contentType).toBe("application/x-ndjson");
    const expectedAuth = `Basic ${Buffer.from(`${USER}:${PASSWORD}`).toString("base64")}`;
    expect(metrics.authorization).toBe(expectedAuth);

    // x-ndjson framing: newline-terminated action line + document line pairs.
    expect(metrics.body.endsWith("\n")).toBe(true);
    const lines = metrics.body.split("\n").filter((l) => l.length > 0);
    expect(lines.length).toBeGreaterThanOrEqual(2);
    expect(lines.length % 2).toBe(0);

    for (let i = 0; i < lines.length; i += 2) {
      const action = JSON.parse(lines[i]);
      // The %(date) placeholder expands to today's UTC date.
      expect(action.index._index).toMatch(/^nsclient_metrics-\d{4}-\d{2}-\d{2}$/);
      // Every document gets its own id (a shared id makes documents in a
      // batch overwrite each other).
      expect(action.index._id).toMatch(/^[0-9a-f-]{36}$/);
      // _type must be absent by default: Elasticsearch 8 rejects it.
      expect(action.index._type).toBeUndefined();

      const doc = JSON.parse(lines[i + 1]);
      expect(doc.hostname).toBeTruthy();
      expect(doc["@timestamp"]).toMatch(/^\d{4}-\d{2}-\d{2}T/);
    }
  });

  it("gives every submitted document a distinct id", async () => {
    // Wait for a second metrics push so there is more than one bulk request
    // to compare ids across.
    const deadline = Date.now() + 60_000;
    const ids = new Set<string>();
    let actions = 0;
    while (Date.now() < deadline && actions < 2) {
      ids.clear();
      actions = 0;
      for (const r of requests) {
        for (const line of r.body.split("\n")) {
          if (!line) continue;
          let parsed: { index?: { _id?: string } };
          try {
            parsed = JSON.parse(line);
          } catch {
            continue;
          }
          if (parsed.index?._id) {
            actions += 1;
            ids.add(parsed.index._id);
          }
        }
      }
      if (actions < 2) await new Promise((r) => setTimeout(r, 500));
    }
    expect(actions).toBeGreaterThanOrEqual(2);
    expect(ids.size).toBe(actions);
  });

  it("forwards the nsclient log to the log index", async () => {
    // Anything the agent logs after the module is loaded is forwarded to the
    // nsclient log index; the boot messages that follow module load are
    // enough. This is the path that used to hardcode TLS verification off -
    // here it just proves the log pipeline reaches the server at all.
    const log = await waitForBulk((r) => r.body.includes("nsclient_log-"));
    expect(log).toBeDefined();
    if (!log) return;
    const lines = log.body.split("\n").filter((l) => l.length > 0);
    const doc = JSON.parse(lines[1]);
    expect(doc.message).toBeTruthy();
    expect(doc.level).toBeTruthy();
    expect(doc.hostname).toBeTruthy();
  });
});
