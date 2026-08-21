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
import { NscpInstance } from "@fixtures/index";
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

  it("reports an unreachable daemon distinctly", async () => {
    const out = await query("check_docker", ["host=/tmp/nscp-no-such-daemon.sock"]);
    expect(out).toMatch(/Failed to connect to docker daemon at '\/tmp\/nscp-no-such-daemon\.sock'/);
  });

  it("refuses a non-local endpoint", async () => {
    const out = await query("check_docker", ["host=../../etc/passwd"]);
    expect(out).toMatch(/Refusing docker endpoint/);
  });
});
