/**
 * Exercises the CheckDocker module end-to-end against the real local docker
 * daemon (the same one the test-suite's testcontainers use).
 *
 * Each case runs a one-shot client query — `nscp client --module CheckDocker
 * --boot --query <cmd> ...` — which passes k=v as single tokens (same as
 * REST). Output is the raw Nagios "message|perfdata" line without a
 * status-word prefix.
 *
 * The module talks to the daemon over the unix socket, so the suite needs the
 * standard /var/run/docker.sock to exist (that is where the module's default
 * endpoint points); Windows named-pipe coverage lives in the unit tests.
 */
import * as fs from "fs";
import request from "supertest";
import { NscpInstance, REST_URL } from "@fixtures/index";
import { skipDocker, GenericContainer, type StartedTestContainer } from "./src/docker";

jest.setTimeout(300_000);

const DOCKER_SOCKET = "/var/run/docker.sock";
const canRun = !skipDocker() && process.platform === "linux" && fs.existsSync(DOCKER_SOCKET);
const maybeDescribe = canRun ? describe : describe.skip;

maybeDescribe("CheckDocker commands", () => {
  let nscp: NscpInstance;
  let probe: StartedTestContainer;
  let probeName = "";

  /** Run a CheckDocker query and return the combined output. */
  async function query(command: string, args: string[] = []): Promise<string> {
    const r = await nscp.run(["client", "--module", "CheckDocker", "--boot", "--query", command, ...args], {
      allowFailure: true,
    });
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    // A long-sleeping container guarantees at least one known running
    // container for the duration of the suite.
    probe = await new GenericContainer("alpine:3").withCommand(["sleep", "600"]).start();
    probeName = probe.getName().replace(/^\//, "");
  });

  afterAll(async () => {
    await probe?.stop();
  });

  it("check_docker lists running containers", async () => {
    const out = await query("check_docker", [`filter=names = '${probeName}'`]);
    expect(out).toMatch(new RegExp(`${probeName}=running`));
    expect(out).toMatch(/^OK/m);
  });

  it("check_docker container= is OK for a running container", async () => {
    const out = await query("check_docker", [`container=${probeName}`]);
    expect(out).toMatch(/^OK/m);
  });

  it("check_docker container= goes CRITICAL for an unknown container", async () => {
    const out = await query("check_docker", ["container=nscp-no-such-container"]);
    expect(out).toMatch(/^CRITICAL/m);
    expect(out).toMatch(/nscp-no-such-container=missing/);
  });

  it("check_docker with a filter that matches nothing takes the empty state", async () => {
    // The #1499 shape: the default crit (`container_state != 'running'`) is
    // force-evaluated with no container bound once nothing matched. The
    // documented empty state is WARNING; the option still relaxes it. The
    // empty syntax carries no status word, so the exit code is the verdict.
    const args = ["client", "--module", "CheckDocker", "--boot", "--query", "check_docker", "filter=names = 'nscp-no-such-container-1499'"];
    const r = await nscp.run(args, { allowFailure: true });
    expect(r.all ?? `${r.stdout}\n${r.stderr}`).toMatch(/No containers found/);
    expect(r.exitCode).toBe(1);
    const relaxed = await nscp.run([...args, "empty-state=ok"], { allowFailure: true });
    expect(relaxed.exitCode).toBe(0);
  });

  it("check_docker_restarts with a filter that matches nothing is OK", async () => {
    const r = await nscp.run(
      ["client", "--module", "CheckDocker", "--boot", "--query", "check_docker_restarts", "filter=names = 'nscp-no-such-container-1499'"],
      { allowFailure: true },
    );
    expect(r.all ?? `${r.stdout}\n${r.stderr}`).toMatch(/No containers found/);
    expect(r.exitCode).toBe(0);
  });

  it("check_docker accepts all=true as a valued boolean (REST k=v token path)", async () => {
    const out = await query("check_docker", ["all=true", `filter=names = '${probeName}'`]);
    expect(out).not.toMatch(/does not take any arguments/i);
    expect(out).toMatch(new RegExp(`${probeName}=running`));
  });

  it("check_docker exposes image/health/ip/created keywords", async () => {
    const out = await query("check_docker", [
      `filter=names = '${probeName}'`,
      "detail-syntax=%(names) image=%(image) state=%(container_state) status=[%(container_status)]",
      "top-syntax=${list}",
    ]);
    // container_status is the human readable status, e.g. "Up 3 hours (healthy)".
    expect(out).toMatch(new RegExp(`${probeName} image=alpine:3 state=running status=\\[Up [^\\]]+\\]`));
  });

  it("check_docker_info reports daemon version and counts with perf data", async () => {
    const out = await query("check_docker_info", ["warning=running < 1"]);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/docker \S+ on \S+: \d+ running, \d+ paused, \d+ stopped containers, \d+ images/);
    expect(out).toMatch(/running'?=\d+/); // perf from the threshold
  });

  it("check_docker_stats samples cpu and memory for a container", async () => {
    const out = await query("check_docker_stats", [
      `container=${probeName}`,
      "warning=memory_pct > 99",
      "critical=cpu_pct > 400",
    ]);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(new RegExp(`${probeName}: cpu \\d+%, memory \\S+ of \\S+ \\(\\d+%\\)`));
    // Thresholded keywords are emitted as perf data.
    expect(out).toMatch(/memory %'?=\d+%/);
  });

  it("check_docker_stats accepts size units in thresholds", async () => {
    // Size literals take a unit (1b, 64k, 1T ...), like check_files' size.
    // A sleeping alpine uses far more than 1 byte and far less than 1T.
    const out = await query("check_docker_stats", [`container=${probeName}`, "warning=memory_used > 1T"]);
    expect(out).toMatch(/^OK/m);
    const out2 = await query("check_docker_stats", [`container=${probeName}`, "warning=memory_used > 1b"]);
    expect(out2).toMatch(/^WARNING/m);
  });

  it("check_docker_restarts reports a stable container as OK", async () => {
    const out = await query("check_docker_restarts", [`container=${probeName}`]);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(new RegExp(`${probeName}: 0 restarts, running`));
  });

  it("check_docker_restarts exposes started/oom keywords", async () => {
    const out = await query("check_docker_restarts", [
      `container=${probeName}`,
      "detail-syntax=%(names) oom=%(oom_killed) exit=%(exit_code)",
      "top-syntax=${list}",
      // started is seconds since start; the probe started this test run.
      "critical=started > 1d",
    ]);
    expect(out).toMatch(new RegExp(`${probeName} oom=0 exit=0`));
  });

  it("check_docker_df reports disk usage with reclaimable space", async () => {
    const out = await query("check_docker_df", ["warning=total_size > 1000T"]);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/images \d+ \(\S+\), containers \d+ \(\S+\), volumes \d+ \(\S+\), build cache \S+, reclaimable \S+/);
    expect(out).toMatch(/total'?=\d+B/); // perf from the threshold
  });

  // `host=` may now only repeat the configured endpoint, and that check runs
  // before the endpoint is validated or connected to. So neither of these gets
  // as far as the connect or the traversal check: both are refused for naming
  // an endpoint the request does not get to choose. The shape-level refusals
  // ("expected an absolute path", "path traversal is not allowed") still guard
  // the *setting* and are covered by docker_endpoint_test; the connect failure
  // message is covered by check_docker_test through its injected fetcher.
  it("refuses a request-supplied endpoint that names an unreachable socket", async () => {
    const out = await query("check_docker", ["host=/tmp/nscp-no-such-daemon.sock"]);
    expect(out).toMatch(/Refusing a request-supplied docker endpoint/);
    expect(out).toMatch(/\[\/settings\/docker\]/);
  });

  it("refuses a request-supplied endpoint that tries to traverse", async () => {
    const out = await query("check_docker", ["host=../../etc/passwd"]);
    expect(out).toMatch(/Refusing a request-supplied docker endpoint/);
  });

  it("does not echo the endpoint the caller asked for", async () => {
    const out = await query("check_docker", ["host=/tmp/nscp-secret-probe.sock"]);
    expect(out).not.toMatch(/nscp-secret-probe/);
  });
});

/**
 * The `docker` fact set, read over REST from an `nscp test` with the three
 * switches on. The inventory the daemon behind /var/run/docker.sock really
 * holds, which is why this lives here rather than in rest-facts.test.ts: the
 * probe container is the one record whose shape is known in advance.
 */
maybeDescribe("CheckDocker facts", () => {
  let nscp: NscpInstance;
  let probe: StartedTestContainer;
  let probeName = "";
  let key: string | undefined = undefined;

  beforeAll(async () => {
    probe = await new GenericContainer("alpine:3").withCommand(["sleep", "600"]).start();
    probeName = probe.getName().replace(/^\//, "");

    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        WEBServer: "enabled",
        CheckDocker: "enabled",
      },
      "/settings/default": {
        "allowed hosts": "127.0.0.1,::1",
      },
      "/settings/WEB/server/users/admin": {
        role: "full",
        password: "default-password",
      },
      "/settings/docker/facts": {
        docker: "true",
        "docker.containers": "true",
        "docker.images": "true",
      },
    });
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
    await request(REST_URL)
      .get("/api/v2/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        key = response.body.key;
      });
  });

  afterAll(async () => {
    await nscp?.stop();
    await probe?.stop();
  });

  /** Collect now and return the whole document: the startup round may still be running. */
  async function refresh(): Promise<Record<string, any>> {
    const response = await request(REST_URL)
      .post("/api/v2/facts/commands/refresh")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
    return response.body;
  }

  it("describes the daemon without its counts", async () => {
    const document = await refresh();
    expect(document.enabled).toContain("docker");
    expect(document.errors.docker).toBeUndefined();
    const docker = document.facts.docker;
    expect(docker.version).toMatch(/^\d+\./);
    expect(docker.os_type).toEqual("linux");
    // The `os` set's spelling, not the kernel's.
    expect(docker.architecture).toMatch(/^[a-z0-9_]+$/);
    expect(docker.architecture).not.toEqual("aarch64");
    if (docker.cpus !== undefined) expect(docker.cpus).toBeGreaterThan(0);
    if (docker.memory_bytes !== undefined) expect(docker.memory_bytes).toBeGreaterThan(0);
    // Inventory, not monitoring: no container or image counts, which
    // check_docker_info reports and which change every round.
    expect(
      Object.keys(docker).filter((k) => /containers|images|running|stopped|paused/.test(k)),
    ).toEqual(["containers", "images"]);
  });

  it("lists the containers by the name check_docker gives them, without state", async () => {
    const document = await refresh();
    const containers: Record<string, any>[] = document.facts.docker.containers;
    expect(Array.isArray(containers)).toBe(true);
    const ids = containers.map((c) => c.id);
    expect(new Set(ids).size).toEqual(ids.length);
    expect([...ids].sort()).toEqual(ids);

    const record = containers.find((c) => c.id === probeName);
    expect(record).toBeDefined();
    expect(record!.container_id).toEqual(probe.getId());
    expect(record!.image).toEqual("alpine:3");
    expect(record!.image_id).toMatch(/^sha256:/);
    expect(record!.created).toMatch(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/);
    for (const container of containers) {
      // No state, status, health or address: those change every round and
      // belong to check_docker.
      expect(Object.keys(container).filter((k) => /state|status|health|^ip$/.test(k))).toEqual([]);
    }

    // The same ids check_docker reports, which is what lets a failing check
    // find its record.
    const check = await request(REST_URL)
      .get(
        "/api/v2/queries/check_docker/commands/execute?all=true&filter=none&warning=none&critical=none&empty-state=ok&top-syntax=${list}&detail-syntax=%25(names)%0A",
      )
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
    const reported = check.body.lines.map((l: { message: string }) => l.message).join("\n");
    for (const container of containers) expect(reported).toContain(container.id);
  });

  it("lists the images by image id, with their tags", async () => {
    const document = await refresh();
    const images: Record<string, any>[] = document.facts.docker.images;
    expect(Array.isArray(images)).toBe(true);
    const ids = images.map((i) => i.id);
    expect(new Set(ids).size).toEqual(ids.length);
    // The probe's image is here, found by its tag.
    const alpine = images.find((i) => (i.tags ?? []).includes("alpine:3"));
    expect(alpine).toBeDefined();
    expect(alpine!.id).toMatch(/^sha256:/);
    expect(alpine!.size_bytes).toBeGreaterThan(0);
    for (const image of images) {
      // Keyed on the image id, which does not move when a tag does; a
      // dangling image carries no tags at all, never a "<none>:<none>" one.
      expect(image.id).toEqual(image.image_id);
      if (image.tags !== undefined)
        expect(image.tags).not.toContain("<none>:<none>");
    }
  });
});
