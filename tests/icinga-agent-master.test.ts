/**
 * IcingaAgent against a real Icinga 2 master.
 *
 * icinga-agent.test.ts drives the agent with a fake master, which can force
 * the cases a real one never produces on demand. This suite is the other half:
 * the official icinga/icinga2 image as the master, configured the way
 * docs/docs/scenarios/icinga-agent.md tells operators to, so a change in what
 * a real Icinga sends - or accepts - shows up here.
 *
 * It needs docker (skipped under NSCP_SKIP_DOCKER=1, as the CI package jobs
 * run). The image defaults to the Icinga release the module was written
 * against; ICINGA2_IMAGE overrides it, e.g. to try a newer release or to pull
 * through a registry mirror when Docker Hub rate-limits:
 *
 *   ICINGA2_IMAGE=mirror.gcr.io/icinga/icinga2:2.16.6 npx jest --runInBand icinga-agent-master
 *
 * Covered: ticket signing over the unverified first connection (the agent
 * stores exactly the master's CA), a verified reconnect, the endpoint seen as
 * connected with the IfwApiCheckCommand capability, check results for an
 * ifw-api command, a plain CheckCommand and the generic pass-through (with
 * Icinga's `$$` escape), the permission policy denying a check as
 * IcingaAgent:<master>, and pinning the master's certificate with
 * `parent fingerprint`.
 */
import * as fs from "fs";
import * as os from "os";
import * as path from "path";
import * as tls from "tls";
import {
  GenericContainer,
  NscpInstance,
  curlGet,
  describeIf,
  moduleBuiltHere,
  skipDocker,
  trackContainerLogs,
  waitForHttp,
  type StartedTestContainer,
} from "@fixtures/index";

jest.setTimeout(600_000);

const IMAGE = process.env.ICINGA2_IMAGE || "icinga/icinga2:2.16.6";
const MASTER = "icinga-master";
const AGENT = "nscp-agent";
const API = { user: "nscp-test", password: "nscp-test" };

// The master side, as the scenario page describes it. conf.d is not in a
// zone, so the objects name the master's zone themselves.
const MASTER_CONF = `
object Endpoint "${AGENT}" { }
object Zone "${AGENT}" {
  endpoints = [ "${AGENT}" ]
  parent = "master"
}

object ApiUser "${API.user}" {
  password = "${API.password}"
  permissions = [ "*" ]
}

object CheckCommand "check_ok" {
  import "ifw-api"
  vars.ifw_api_arguments = {
    "message" = { value = "$nscp_message$" }
  }
}

object CheckCommand "check_warning" {
  import "plugin-check-command"
  command = [ "/bin/false" ]  // never run on the master
}

object CheckCommand "check_critical" {
  import "ifw-api"
}

object CheckCommand "nscp-query" {
  import "ifw-api"
  vars.ifw_api_command = "$nscp_query$"
  vars.ifw_api_arguments = {
    "(arguments)" = { value = "$nscp_arguments$", skip_key = true }
  }
}

object Host "${AGENT}" {
  zone = "master"
  check_command = "cluster-zone"
  vars.cluster_zone = name
  check_interval = 5s
  retry_interval = 5s
}

template Service "agent-service" {
  host_name = "${AGENT}"
  zone = "master"
  command_endpoint = "${AGENT}"
  check_interval = 5s
  retry_interval = 5s
}

object Service "ifw-api" {
  import "agent-service"
  check_command = "check_ok"
  vars.nscp_message = "hello from icinga"
}

object Service "plain" {
  import "agent-service"
  check_command = "check_warning"
}

object Service "pass-through" {
  import "agent-service"
  check_command = "nscp-query"
  vars.nscp_query = "check_ok"
  vars.nscp_arguments = [ "message=price $$5" ]
}

object Service "denied" {
  import "agent-service"
  check_command = "check_critical"
}
`;

interface ServiceState {
  name: string;
  state: number;
  output: string;
  checkSource: string;
}

function sleep(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}

/** The SHA-256 fingerprint of the certificate a TLS server presents. */
function serverFingerprint(port: number): Promise<string> {
  return new Promise((resolve, reject) => {
    const socket = tls.connect({ host: "127.0.0.1", port, rejectUnauthorized: false }, () => {
      const fingerprint = socket.getPeerCertificate().fingerprint256;
      socket.end();
      resolve(fingerprint);
    });
    socket.on("error", reject);
  });
}

const enabled = !skipDocker() && moduleBuiltHere("IcingaAgent") && moduleBuiltHere("CheckHelpers");

describeIf(enabled)(`IcingaAgent against a real Icinga 2 master (${IMAGE})`, () => {
  let master: StartedTestContainer;
  let confDir: string;
  let port: number;
  let api: string;

  async function apiGet(
    pathAndQuery: string,
  ): Promise<{ results: { name: string; attrs: Record<string, any> }[] }> {
    return JSON.parse(await curlGet(`${api}${pathAndQuery}`, { auth: API }));
  }

  async function services(): Promise<ServiceState[]> {
    const body = await apiGet(
      `/v1/objects/services?filter=host.name==%22${AGENT}%22&attrs=name&attrs=state&attrs=last_check_result`,
    );
    return body.results.map((r) => ({
      name: r.attrs.name,
      state: r.attrs.state,
      output: r.attrs.last_check_result?.output ?? "",
      checkSource: r.attrs.last_check_result?.check_source ?? "",
    }));
  }

  /** Wait until every service has a result from the agent, newer than `since`. */
  async function resultsFromAgent(timeoutMs = 120_000): Promise<Map<string, ServiceState>> {
    const deadline = Date.now() + timeoutMs;
    let last: ServiceState[] = [];
    while (Date.now() < deadline) {
      last = await services();
      if (last.length > 0 && last.every((s) => s.checkSource === AGENT))
        return new Map(last.map((s) => [s.name, s]));
      await sleep(2_000);
    }
    throw new Error(`no results from the agent for every service: ${JSON.stringify(last)}`);
  }

  async function ticket(): Promise<string> {
    const r = await master.exec(["icinga2", "pki", "ticket", "--cn", AGENT]);
    if (r.exitCode !== 0) throw new Error(`icinga2 pki ticket failed: ${r.output}`);
    return r.output.trim();
  }

  async function startAgent(
    settings: Record<string, string>,
  ): Promise<{ nscp: NscpInstance; certDir: string }> {
    const nscp = new NscpInstance();
    const certDir = path.join(nscp.workDir, "icinga-certs");
    await nscp.configure({
      "/modules": { IcingaAgent: "enabled", CheckHelpers: "enabled" },
      "/settings/log": { level: "debug" },
      "/settings/Icinga/agent": {
        "parent host": "127.0.0.1",
        "parent port": String(port),
        "parent endpoint": MASTER,
        "endpoint name": AGENT,
        "certificate directory": certDir,
        ...settings,
      },
      "/settings/permissions": { enabled: "true" },
      "/settings/permissions/policies": {
        [`IcingaAgent:${MASTER}`]: "CheckHelpers.check_ok, CheckHelpers.check_warning",
      },
    });
    nscp.start();
    return { nscp, certDir };
  }

  beforeAll(async () => {
    confDir = fs.mkdtempSync(path.join(os.tmpdir(), "icinga-master-conf-"));
    fs.writeFileSync(path.join(confDir, "nscp-agent.conf"), MASTER_CONF);
    fs.chmodSync(confDir, 0o755);
    fs.chmodSync(path.join(confDir, "nscp-agent.conf"), 0o644);
    master = await trackContainerLogs(
      await new GenericContainer(IMAGE)
        .withHostname(MASTER)
        .withEnvironment({ ICINGA_MASTER: "1", ICINGA_CN: MASTER })
        .withBindMounts([
          {
            source: path.join(confDir, "nscp-agent.conf"),
            target: "/etc/icinga2/conf.d/nscp-agent.conf",
            mode: "ro",
          },
        ])
        .withExposedPorts(5665)
        .start(),
      "icinga2-master",
    );
    port = master.getMappedPort(5665);
    api = `https://127.0.0.1:${port}`;
    await waitForHttp(`${api}/v1/status`, { auth: API, timeoutMs: 120_000 });
  });

  afterAll(async () => {
    await master?.stop();
    fs.rmSync(confDir, { recursive: true, force: true });
  });

  describe("with a ticket (trust on first use)", () => {
    let nscp: NscpInstance;
    let certDir: string;
    let results: Map<string, ServiceState>;

    beforeAll(async () => {
      ({ nscp, certDir } = await startAgent({ ticket: await ticket() }));
      results = await resultsFromAgent();
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("gets its certificate signed and stores the master's CA", async () => {
      const masterCa = (await master.exec(["cat", "/var/lib/icinga2/certs/ca.crt"])).output;
      expect(fs.readFileSync(path.join(certDir, "ca.crt"), "utf8").trim()).toBe(masterCa.trim());
      expect(nscp.capturedStdout()).toContain("stored the certificate signed by the Icinga CA");
    });

    it("is seen by the master as a connected endpoint with the ifw-api capability", async () => {
      const endpoint = (
        await apiGet(`/v1/objects/endpoints/${AGENT}?attrs=connected&attrs=capabilities`)
      ).results[0].attrs;
      expect(endpoint.connected).toBe(true);
      expect(endpoint.capabilities).toBe(2);
      const host = (await apiGet(`/v1/objects/hosts/${AGENT}?attrs=state`)).results[0].attrs;
      expect(host.state).toBe(0);
    });

    it("runs an ifw-api CheckCommand with its arguments", () => {
      expect(results.get("ifw-api")).toMatchObject({ state: 0, output: "hello from icinga" });
    });

    it("runs a plain CheckCommand by its name", () => {
      expect(results.get("plain")?.state).toBe(1);
    });

    it("passes NSClient++ arguments through, with Icinga's $$ escape undone", () => {
      expect(results.get("pass-through")).toMatchObject({ state: 0, output: "price $5" });
    });

    it("runs checks as IcingaAgent:<master> under the permission policy", () => {
      const denied = results.get("denied");
      expect(denied?.state).toBe(3);
      expect(denied?.output).toContain(`IcingaAgent:${MASTER}`);
    });
  });

  describe("with the master's certificate pinned", () => {
    let nscp: NscpInstance;

    afterAll(async () => {
      await nscp?.stop();
    });

    it("verifies the master on the first connection", async () => {
      const fingerprint = await serverFingerprint(port);
      ({ nscp } = await startAgent({ ticket: await ticket(), "parent fingerprint": fingerprint }));
      const deadline = Date.now() + 60_000;
      while (
        Date.now() < deadline &&
        !nscp.capturedStdout().includes("stored the certificate signed by the Icinga CA")
      )
        await sleep(500);
      expect(nscp.capturedStdout()).toContain(
        "verifying the parent by its pinned `parent fingerprint`",
      );
      expect(nscp.capturedStdout()).toContain("stored the certificate signed by the Icinga CA");
      expect(nscp.capturedStdout()).not.toContain("NOT verified");
    });
  });
});
