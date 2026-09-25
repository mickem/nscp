/**
 * Exercises the CheckKubernetes module end-to-end against a fake Kubernetes
 * API server: an HTTPS listener in Node, signed by a throwaway CA, that
 * requires the configured bearer token and serves canned API objects with
 * pagination. That keeps the suite deterministic on every CI runner with no
 * cluster; the same commands against a real cluster live in the live suite
 * gated on NSCP_KUBECONFIG.
 *
 * Most cases run a one-shot client query - `nscp client --module
 * CheckKubernetes --boot --query <cmd> k=v ...` - which passes k=v as single
 * tokens, the same shape REST uses. Its output is the raw Nagios
 * "message|perfdata" line without a status-word prefix. One case per command
 * goes over the REST API of a long-lived `nscp test`, which is where argument
 * quoting differs from the CLI.
 */
import * as https from "https";
import type { AddressInfo } from "net";
import type { IncomingMessage, ServerResponse } from "http";

import {
  CRITICAL,
  NscpInstance,
  OK,
  UNKNOWN,
  WARNING,
  executeQuery,
  generateCertChain,
  messageOf,
  perfOf,
  setupQueryNscp,
} from "@fixtures/index";

jest.setTimeout(180_000);

const TOKEN = "nscp-test-token-3f9a";

// --- the objects the fake API server serves ---------------------------------

const VERSION = { major: "1", minor: "30", gitVersion: "v1.30.2", platform: "linux/amd64" };

function node(name: string, ready: string, extra: Record<string, unknown> = {}) {
  return {
    metadata: { name, creationTimestamp: "2024-01-01T00:00:00Z", labels: {} },
    spec: {},
    status: {
      conditions: [
        { type: "MemoryPressure", status: "False" },
        { type: "DiskPressure", status: "False" },
        { type: "PIDPressure", status: "False" },
        { type: "Ready", status: ready },
      ],
      nodeInfo: { kubeletVersion: "v1.30.2", osImage: "Ubuntu 22.04.4 LTS", architecture: "amd64" },
      capacity: { cpu: "4", memory: "16Gi", pods: "110" },
      allocatable: { cpu: "3800m", memory: "15Gi", pods: "110" },
    },
    ...extra,
  };
}

const NODES = [
  node("cp-1", "True", {
    metadata: { name: "cp-1", labels: { "node-role.kubernetes.io/control-plane": "" } },
  }),
  node("worker-1", "False"),
  node("worker-2", "True", { spec: { unschedulable: true } }),
];

function runningPod(name: string, namespace: string) {
  return {
    metadata: { name, namespace, creationTimestamp: "2024-01-01T00:00:00Z", labels: { app: name } },
    spec: { nodeName: "worker-1", containers: [{ name }] },
    status: {
      phase: "Running",
      podIP: "10.244.1.5",
      qosClass: "Burstable",
      conditions: [{ type: "Ready", status: "True" }],
      containerStatuses: [{ name, ready: true, restartCount: 0, state: { running: {} } }],
    },
  };
}

const CRASHING_POD = {
  metadata: {
    name: "api-5f6c7-xyz12",
    namespace: "shop",
    creationTimestamp: "2026-09-25T00:00:00Z",
  },
  spec: { nodeName: "worker-2", containers: [{ name: "api" }] },
  status: {
    phase: "Running",
    conditions: [{ type: "Ready", status: "False" }],
    containerStatuses: [
      {
        name: "api",
        ready: false,
        restartCount: 7,
        state: { waiting: { reason: "CrashLoopBackOff" } },
        lastState: { terminated: { exitCode: 1, reason: "Error" } },
      },
    ],
  },
};

const FINISHED_POD = {
  metadata: { name: "backup-28800-q9x", namespace: "ops" },
  spec: { containers: [{ name: "backup" }] },
  status: {
    phase: "Succeeded",
    containerStatuses: [
      {
        name: "backup",
        ready: false,
        restartCount: 0,
        state: { terminated: { exitCode: 0, reason: "Completed" } },
      },
    ],
  },
};

// Page one carries a continue token; page two holds the rest.
const PODS_PAGE_1 = [runningPod("web-7d4b9c-abcde", "shop"), CRASHING_POD];
const PODS_PAGE_2 = [FINISHED_POD, runningPod("cache-0", "shop")];

const DEPLOYMENTS = [
  {
    metadata: { name: "web", namespace: "shop" },
    spec: { replicas: 3 },
    status: { readyReplicas: 3, availableReplicas: 3, updatedReplicas: 3 },
  },
  {
    metadata: { name: "api", namespace: "shop" },
    spec: { replicas: 3 },
    status: { readyReplicas: 1, availableReplicas: 1, updatedReplicas: 3, unavailableReplicas: 2 },
  },
  {
    metadata: { name: "legacy", namespace: "ops" },
    spec: { replicas: 2 },
    status: { unavailableReplicas: 2 },
  },
];
const STATEFULSETS = [
  {
    metadata: { name: "db", namespace: "shop" },
    spec: { replicas: 3 },
    status: { readyReplicas: 3, availableReplicas: 3, updatedReplicas: 3 },
  },
];
const DAEMONSETS = [
  {
    metadata: { name: "node-exporter", namespace: "monitoring" },
    spec: {},
    status: {
      desiredNumberScheduled: 3,
      numberReady: 2,
      numberAvailable: 2,
      updatedNumberScheduled: 3,
      numberUnavailable: 1,
    },
  },
];

// --- the fake API server ------------------------------------------------------

interface FakeApiServer {
  port: number;
  url: string;
  /** Every request seen: method, path and the Authorization header. */
  requests: { path: string; authorization: string | undefined }[];
  /** What /readyz answers; flipped by a test. */
  ready: boolean;
  close: () => Promise<void>;
}

function status(res: ServerResponse, code: number, message: string, reason: string): void {
  res.writeHead(code, { "Content-Type": "application/json" });
  res.end(
    JSON.stringify({ kind: "Status", apiVersion: "v1", status: "Failure", message, reason, code }),
  );
}

function list(res: ServerResponse, items: unknown[], cont = ""): void {
  res.writeHead(200, { "Content-Type": "application/json" });
  res.end(
    JSON.stringify({
      kind: "List",
      apiVersion: "v1",
      metadata: { resourceVersion: "1", continue: cont },
      items,
    }),
  );
}

function startFakeApiServer(cert: { keyPem: string; certPem: string }): Promise<FakeApiServer> {
  const state = { requests: [] as FakeApiServer["requests"], ready: true };
  const handler = (req: IncomingMessage, res: ServerResponse) => {
    const url = new URL(req.url ?? "/", "https://localhost");
    state.requests.push({
      path: url.pathname + url.search,
      authorization: req.headers.authorization,
    });
    if (req.headers.authorization !== `Bearer ${TOKEN}`)
      return status(res, 401, "Unauthorized", "Unauthorized");
    const ns = url.pathname.match(/^\/api\/v1\/namespaces\/([^/]+)\/pods$/)?.[1];
    switch (true) {
      case url.pathname === "/version":
        res.writeHead(200, { "Content-Type": "application/json" });
        return res.end(JSON.stringify(VERSION));
      case url.pathname === "/readyz":
        res.writeHead(state.ready ? 200 : 500, { "Content-Type": "text/plain" });
        return res.end(
          state.ready ? "ok" : "[+]ping ok\n[-]etcd failed: reason withheld\nreadyz check failed\n",
        );
      case url.pathname === "/api/v1/nodes":
        return list(res, NODES);
      case url.pathname === "/api/v1/pods":
        // Two pages, so the check has to follow metadata.continue.
        if (url.searchParams.get("continue") === "page-2") return list(res, PODS_PAGE_2);
        return list(res, PODS_PAGE_1, "page-2");
      case ns === "secret":
        return status(
          res,
          403,
          `pods is forbidden: User "system:serviceaccount:monitoring:nscp" cannot list resource "pods" in API group "" in the namespace "secret"`,
          "Forbidden",
        );
      case ns !== undefined:
        return list(
          res,
          [...PODS_PAGE_1, ...PODS_PAGE_2].filter((p) => p.metadata.namespace === ns),
        );
      case url.pathname === "/apis/apps/v1/deployments":
        return list(res, DEPLOYMENTS);
      case url.pathname === "/apis/apps/v1/statefulsets":
        return list(res, STATEFULSETS);
      case url.pathname === "/apis/apps/v1/daemonsets":
        return list(res, DAEMONSETS);
      case url.pathname.startsWith("/apis/metrics.k8s.io/"):
        return status(res, 404, "the server could not find the requested resource", "NotFound");
      default:
        return status(res, 404, "the server could not find the requested resource", "NotFound");
    }
  };
  return new Promise((resolve) => {
    const srv = https.createServer({ key: cert.keyPem, cert: cert.certPem }, handler);
    srv.listen(0, "127.0.0.1", () => {
      const port = (srv.address() as AddressInfo).port;
      resolve({
        port,
        url: `https://127.0.0.1:${port}`,
        requests: state.requests,
        get ready() {
          return state.ready;
        },
        set ready(v: boolean) {
          state.ready = v;
        },
        close: () => new Promise<void>((res) => srv.close(() => res())),
      });
    });
  });
}

// --- the suite ----------------------------------------------------------------

describe("CheckKubernetes commands", () => {
  let nscp: NscpInstance;
  let api: FakeApiServer;
  let key: string;
  let caPath: string;

  /** Run a CheckKubernetes query through the one-shot client and return its output. */
  async function query(
    command: string,
    args: string[] = [],
    instance: NscpInstance = nscp,
  ): Promise<{ out: string; exitCode: number }> {
    const r = await instance.run(
      ["client", "--module", "CheckKubernetes", "--boot", "--query", command, ...args],
      { allowFailure: true },
    );
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, exitCode: r.exitCode ?? -1 };
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    // A CA-signed server certificate with SAN DNS:localhost + IP:127.0.0.1,
    // so `verify mode = peer` (chain and host name) passes against the CA.
    const bundle = generateCertChain({
      outDir: nscp.scratch("kube_certs"),
      signed: { server: { commonName: "localhost", isServer: true } },
    });
    caPath = bundle.ca.certPath;
    api = await startFakeApiServer(bundle.signed.server);
    key = await setupQueryNscp(nscp, "CheckKubernetes", {
      "/settings/kubernetes": {
        "api server": api.url,
        token: TOKEN,
        ca: caPath,
        "verify mode": "peer",
      },
    });
  });

  afterAll(async () => {
    await nscp?.stop();
    await api?.close();
  });

  beforeEach(() => {
    api.requests.length = 0;
    api.ready = true;
  });

  it("check_kubernetes reports the version, readiness and node counts", async () => {
    const { out, exitCode } = await query("check_kubernetes");
    expect(out).toMatch(`Kubernetes v1.30.2 at ${api.url}: API ready, 2/3 nodes ready`);
    expect(exitCode).toBe(0);
    expect(api.requests.map((r) => r.path)).toEqual([
      "/version",
      "/readyz",
      "/api/v1/nodes?limit=500",
    ]);
    expect(api.requests.every((r) => r.authorization === `Bearer ${TOKEN}`)).toBe(true);
  });

  it("check_kubernetes goes CRITICAL when /readyz fails and names the check", async () => {
    api.ready = false;
    const { out, exitCode } = await query("check_kubernetes", [
      "detail-syntax=API %(api_ready): %(readyz)",
    ]);
    expect(out).toMatch(/API not ready: failed: etcd/);
    expect(exitCode).toBe(2);
  });

  it("check_kubernetes over REST thresholds the node counts with perf data", async () => {
    const q = await executeQuery(key, "check_kubernetes", {
      warning: "nodes_not_ready > 0",
      critical: "nodes_ready < 1",
    });
    expect(q.result).toBe(WARNING);
    expect(messageOf(q)).toMatch(/2\/3 nodes ready/);
    expect(perfOf(q)[`${api.url} not ready nodes`]?.value).toBe(1);
  });

  it("check_pods follows pagination and flags the crash loop with the defaults", async () => {
    const { out, exitCode } = await query("check_pods");
    expect(out).toMatch(/shop\/api-5f6c7-xyz12=CrashLoopBackOff/);
    expect(out).not.toMatch(/backup/); // Succeeded is filtered out
    expect(exitCode).toBe(2);
    expect(api.requests.map((r) => r.path)).toEqual([
      "/api/v1/pods?limit=500",
      "/api/v1/pods?limit=500&continue=page-2",
    ]);
  });

  it("check_pods pod= finds a pod on the second page and reports a missing one", async () => {
    const found = await query("check_pods", ["pod=cache-0"]);
    expect(found.out).toMatch(/All 1 pods are fine/);
    expect(found.exitCode).toBe(0);
    const missing = await query("check_pods", ["pod=cache-0", "pod=shop/ghost"]);
    expect(missing.out).toMatch(/shop\/ghost=missing/);
    expect(missing.exitCode).toBe(2);
  });

  it("check_pods namespace= and selectors reach the server as query parameters", async () => {
    const { out, exitCode } = await query("check_pods", [
      "namespace=ops",
      "label-selector=app=web,tier!=cache",
      "filter=phase != 'Running'",
    ]);
    expect(out).toMatch(/All 1 pods are fine/);
    expect(exitCode).toBe(0);
    expect(api.requests[0].path).toBe(
      "/api/v1/namespaces/ops/pods?limit=500&labelSelector=app%3Dweb%2Ctier%21%3Dcache",
    );
  });

  it("check_pods over REST takes a filter with spaces and quotes and exposes the keywords", async () => {
    // A tripped threshold, so the detail line (not the OK summary) is rendered.
    const q = await executeQuery(key, "check_pods", {
      filter: "name like 'web'",
      warning: "restarts >= 0",
      "detail-syntax":
        "%(namespace)/%(name) %(pod_status) %(ready_containers)/%(containers) restarts=%(restarts) owner=%(owner_kind) qos=%(qos) ip=%(ip)",
      "top-syntax": "${status}: ${list}",
    });
    expect(q.result).toBe(WARNING);
    expect(messageOf(q)).toBe(
      "WARNING: shop/web-7d4b9c-abcde Running 1/1 restarts=0 owner= qos=Burstable ip=10.244.1.5",
    );
  });

  it("check_pods reports an RBAC denial as UNKNOWN with the rule to grant", async () => {
    const { out, exitCode } = await query("check_pods", ["namespace=secret"]);
    expect(out).toMatch(/denied GET \/api\/v1\/namespaces\/secret\/pods/);
    expect(out).toMatch(/cannot list resource "pods" in API group "" in the namespace "secret"/);
    expect(out).toMatch(/grant the agent's service account get and list/);
    expect(out).not.toContain(TOKEN);
    expect(exitCode).toBe(3);
  });

  it("check_nodes flags the NotReady and cordoned nodes with the defaults", async () => {
    const { out, exitCode } = await query("check_nodes");
    expect(out).toMatch(/worker-1=NotReady, worker-2=Ready,SchedulingDisabled/);
    expect(exitCode).toBe(2);
  });

  it("check_nodes over REST exposes capacity in millicores and bytes with unit thresholds", async () => {
    const q = await executeQuery(key, "check_nodes", {
      node: "cp-1",
      "detail-syntax":
        "%(name) roles=%(roles) cpu=%(cpu_allocatable) mem=%(memory_allocatable) pods=%(pods_capacity)",
      warning: "memory_allocatable > 1G",
      critical: "ready != 'True'",
    });
    expect(q.result).toBe(WARNING);
    expect(messageOf(q)).toBe(
      "WARNING: cp-1 roles=control-plane cpu=3800 mem=16106127360 pods=110",
    );
    expect(perfOf(q)["cp-1 memory allocatable"]?.value).toBe(16106127360);
  });

  it("check_workloads normalises the three kinds and kind= restricts the calls", async () => {
    const all = await query("check_workloads");
    expect(all.out).toMatch(/Deployment shop\/api=1\/3/);
    expect(all.out).toMatch(/Deployment ops\/legacy=0\/2/);
    expect(all.out).toMatch(/DaemonSet monitoring\/node-exporter=2\/3/);
    expect(all.exitCode).toBe(2);
    expect(api.requests.map((r) => r.path)).toEqual([
      "/apis/apps/v1/deployments?limit=500",
      "/apis/apps/v1/statefulsets?limit=500",
      "/apis/apps/v1/daemonsets?limit=500",
    ]);

    api.requests.length = 0;
    const one = await query("check_workloads", ["kind=statefulset"]);
    expect(one.out).toMatch(/All 1 workloads are available/);
    expect(one.exitCode).toBe(0);
    expect(api.requests.map((r) => r.path)).toEqual(["/apis/apps/v1/statefulsets?limit=500"]);
  });

  it("check_workloads over REST thresholds missing replicas with perf data", async () => {
    const q = await executeQuery(key, "check_workloads", {
      kind: "deployment",
      warning: "missing > 1",
      critical: "available < 1",
    });
    expect(q.result).toBe(CRITICAL);
    expect(messageOf(q)).toMatch(/Deployment shop\/api=1\/3, Deployment ops\/legacy=0\/2/);
    expect(perfOf(q)["shop/api missing"]?.value).toBe(2);
  });

  it("a rejected token is UNKNOWN and the token never appears in the output", async () => {
    const wrong = new NscpInstance();
    await wrong.configure({
      "/modules": { CheckKubernetes: "enabled" },
      "/settings/kubernetes": {
        "api server": api.url,
        token: "not-the-token",
        ca: caPath,
        "verify mode": "peer",
      },
    });
    const { out, exitCode } = await query("check_kubernetes", [], wrong);
    expect(out).toMatch(/rejected the credentials \(HTTP 401 for GET \/version\)/);
    expect(out).not.toContain("not-the-token");
    expect(exitCode).toBe(3);
  });

  it("an untrusted server certificate is refused before the token is sent", async () => {
    // A CA that did not sign the server certificate: verification fails in
    // the handshake, so the request (and the bearer token in it) never
    // leaves the agent.
    const other = new NscpInstance();
    const otherBundle = generateCertChain({
      outDir: other.scratch("other_certs"),
      caCommonName: "some-other-ca",
      signed: { client: { commonName: "unused" } },
    });
    await other.configure({
      "/modules": { CheckKubernetes: "enabled" },
      "/settings/kubernetes": {
        "api server": api.url,
        token: TOKEN,
        ca: otherBundle.ca.certPath,
        "verify mode": "peer",
      },
    });
    const { out, exitCode } = await query("check_kubernetes", [], other);
    expect(out).toMatch(/Failed to connect to Kubernetes API server at/);
    expect(out).not.toContain(TOKEN);
    expect(exitCode).toBe(3);
    expect(api.requests).toHaveLength(0);
  });

  it("an unreachable API server is UNKNOWN with the address", async () => {
    const down = new NscpInstance();
    await down.configure({
      "/modules": { CheckKubernetes: "enabled" },
      "/settings/kubernetes": { "api server": "https://127.0.0.1:1", token: TOKEN },
    });
    const { out, exitCode } = await query("check_nodes", [], down);
    expect(out).toMatch(
      /Failed to connect to Kubernetes API server at 'https:\/\/127\.0\.0\.1:1' \(settings\)/,
    );
    expect(exitCode).toBe(3);
  });

  it("no configured cluster is UNKNOWN with the settings to fill in", async () => {
    const empty = new NscpInstance();
    await empty.configure({ "/modules": { CheckKubernetes: "enabled" } });
    const { out, exitCode } = await query("check_pods", [], empty);
    expect(out).toMatch(/No Kubernetes API server configured: set `api server` and `token`/);
    expect(exitCode).toBe(3);
  });

  it("a check request cannot point the agent at another server", async () => {
    // The endpoint is an operator decision: none of the commands takes a url,
    // host or token argument, so a caller holding queries.execute cannot
    // redirect the bearer token.
    for (const command of ["check_kubernetes", "check_pods", "check_nodes", "check_workloads"]) {
      for (const arg of ["url=https://evil.example.com", "host=evil.example.com", "token=x"]) {
        const { out, exitCode } = await query(command, [arg]);
        expect(out).toMatch(/unrecognised option|unknown option|Unknown option/i);
        expect(exitCode).toBe(3);
      }
    }
    expect(api.requests).toHaveLength(0);
  });
});
