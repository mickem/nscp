/**
 * Exercises the CheckMySQL module end-to-end against a real MariaDB server
 * started with testcontainers.
 *
 * Each case runs a one-shot client query — `nscp client --module CheckMySQL
 * --boot --query <cmd> ...` — which passes k=v as single tokens (same as
 * REST), so this exercises the same argument parsing the REST API uses.
 * Output is the raw Nagios "message|perfdata" line without a status-word
 * prefix.
 *
 * The module is optional (it needs MariaDB Connector/C at build time), so the
 * suite skips itself when the binary was built without it. It also needs a
 * docker daemon for the server container (NSCP_SKIP_DOCKER=1 skips).
 */
import * as fs from "fs";
import * as path from "path";
import { NscpInstance } from "@fixtures/index";
import { dockerOrSkip, GenericContainer, Wait, type StartedTestContainer } from "./src/docker";

jest.setTimeout(300_000);

const ROOT_PASSWORD = "nscp-integration-test";

function moduleBuilt(): boolean {
  if (!process.env.NSCP_BIN) return false;
  const dir = path.join(path.dirname(process.env.NSCP_BIN), "modules");
  return (
    fs.existsSync(path.join(dir, "libCheckMySQL.so")) ||
    fs.existsSync(path.join(dir, "CheckMySQL.dll")) ||
    fs.existsSync(path.join(dir, "CheckMySQL.so"))
  );
}

dockerOrSkip()("CheckMySQL commands", () => {
  let nscp: NscpInstance;
  let mariadb: StartedTestContainer | undefined;
  let host = "";
  let port = 0;

  /** Run a CheckMySQL query and return the combined output. */
  async function query(command: string, args: string[] = []): Promise<string> {
    const r = await nscp.run(["client", "--module", "CheckMySQL", "--boot", "--query", command, ...args], {
      allowFailure: true,
    });
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  /** Connection arguments for the containerized server. */
  function conn(): string[] {
    return [`host=${host}`, `port=${port}`, "user=root", `password=${ROOT_PASSWORD}`];
  }

  beforeAll(async () => {
    if (!moduleBuilt()) return; // cases below turn into cheap no-ops via guards
    nscp = new NscpInstance();
    // The mariadb entrypoint starts a bootstrap server first; "ready for
    // connections" only counts when the real server (with networking) is up,
    // which is the second occurrence.
    mariadb = await new GenericContainer("mariadb:11")
      .withEnvironment({ MARIADB_ROOT_PASSWORD: ROOT_PASSWORD })
      .withExposedPorts(3306)
      .withWaitStrategy(Wait.forLogMessage(/ready for connections/, 2))
      .start();
    host = mariadb.getHost();
    port = mariadb.getMappedPort(3306);
  });

  afterAll(async () => {
    await mariadb?.stop();
  });

  it("check_mysql reports a healthy server with flavor, uptime and connections", async () => {
    if (!moduleBuilt()) return;
    const out = await query("check_mysql", conn());
    expect(out).toMatch(/mariadb \d+\.\d+.*MariaDB/i);
    expect(out).toMatch(/uptime \d+s/);
    expect(out).toMatch(/connections \d+\/\d+ \(\d+%\)/);
  });

  it("check_mysql uptime threshold trips and emits perf data", async () => {
    if (!moduleBuilt()) return;
    // The container started seconds ago, so warning on "< 1000d" must trip.
    const out = await query("check_mysql", [...conn(), "warning=uptime < 1000d"]);
    expect(out).toMatch(/^WARNING/m);
    expect(out).toMatch(/uptime'?=\d+s/);
  });

  it("check_mysql accepts tls=true as a valued boolean (REST k=v token path)", async () => {
    if (!moduleBuilt()) return;
    // The container may or may not speak TLS; the regression under test is the
    // argument parsing (bool_switch would reject the k=v form outright).
    const out = await query("check_mysql", [...conn(), "tls=true"]);
    expect(out).not.toMatch(/does not take any arguments/i);
  });

  it("check_mysql_query exposes result columns as keywords with thresholds", async () => {
    if (!moduleBuilt()) return;
    const out = await query("check_mysql_query", [
      ...conn(),
      "query=SELECT COUNT(*) AS tables_count FROM information_schema.tables",
      "critical=tables_count > 5",
      "detail-syntax=tables=%(tables_count)",
      "top-syntax=${list}",
      // Like CheckWMI/CheckMSSQL, generic query checks emit perf data only
      // once a perf-syntax is chosen (there is no meaningful default alias).
      "perf-syntax=tables",
    ]);
    // information_schema alone holds far more than 5 tables.
    expect(out).toMatch(/tables=\d\d+/);
    expect(out).toMatch(/'tables_counttables'=\d+;0;5/); // perf data from the threshold
  });

  it("check_mysql_query rejects a missing query with a clear message", async () => {
    if (!moduleBuilt()) return;
    const out = await query("check_mysql_query", conn());
    expect(out).toMatch(/No query specified/);
  });

  it("rejects bad credentials with the connect-failure contract", async () => {
    if (!moduleBuilt()) return;
    const out = await query("check_mysql", [`host=${host}`, `port=${port}`, "user=root", "password=wrong"]);
    expect(out).toMatch(/Failed to connect to MySQL server/);
    expect(out).toMatch(/Access denied/);
  });

  it("reports an unreachable server distinctly", async () => {
    if (!moduleBuilt()) return;
    const out = await query("check_mysql", ["host=127.0.0.1", "port=1", "timeout=2"]);
    expect(out).toMatch(/Failed to connect to MySQL server '127\.0\.0\.1:1'/);
  });
});
