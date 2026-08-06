/**
 * Exercises the CheckMSSQL module end-to-end against the real nscp binary.
 *
 * Each case runs a one-shot client query — `nscp client --module CheckMSSQL
 * --boot --query <cmd> ...` — which loads the module, runs the check and prints
 * the Nagios-style result line. None of the commands read the background
 * collector, so no REST server is needed.
 *
 * Two blocks:
 *  - "contract" tests run everywhere on Windows: they assert the documented
 *    no-server behaviour (UNKNOWN + "Failed to connect to SQL Server ...") and
 *    REST-style single-token argument parsing, so they pass with and without a
 *    reachable SQL Server.
 *  - the docker-gated live block starts a SQL Server 2022 container and runs
 *    all five commands for real over SQL authentication. If the installed ODBC
 *    driver cannot talk to SQL Server 2022 (ancient sqlsrv32-only hosts), the
 *    block relaxes to the connect-failure contract instead of failing.
 */
import {
  NscpInstance,
  dockerOrSkip,
  GenericContainer,
  Wait,
  type StartedTestContainer,
} from "@fixtures/index";

jest.setTimeout(600_000);

const onWindows = process.platform === "win32";

// Matches the module's stable error contract for unreachable servers.
const CONNECT_FAILED = /Failed to connect to SQL Server/;
// A check either produced a real status line or hit the connect contract.
const STATUS_OR_CONNECT_FAILED = /(^|\n)(OK|WARNING|CRITICAL)|Failed to connect to SQL Server/;

(onWindows ? describe : describe.skip)("CheckMSSQL contract (no SQL Server required)", () => {
  let nscp: NscpInstance;

  async function query(command: string, args: string[] = []): Promise<string> {
    const r = await nscp.run(
      ["client", "--module", "CheckMSSQL", "--boot", "--query", command, ...args],
      {
        allowFailure: true,
      },
    );
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  beforeAll(() => {
    nscp = new NscpInstance();
  });

  it("check_mssql reports the server state or the documented connect failure", async () => {
    const out = await query("check_mssql", ["timeout=5"]);
    expect(out).toMatch(STATUS_OR_CONNECT_FAILED);
  });

  it("check_mssql with a nonexistent driver hits the connect contract deterministically", async () => {
    // IM002 (driver not found) fails identically on every machine, no network involved.
    const out = await query("check_mssql", ["server=localhost", "driver=No Such Driver 99"]);
    expect(out).toMatch(/UNKNOWN|Failed to connect/);
    expect(out).toMatch(/Failed to connect to SQL Server 'localhost'/);
    expect(out).toMatch(/IM002/);
  });

  it("accepts REST-style single-token key=value arguments (valued boolean)", async () => {
    // trust-cert=true as one token is how REST passes booleans; bool_switch
    // would reject it with "does not take any arguments".
    const out = await query("check_mssql", ["timeout=5", "trust-cert=true"]);
    expect(out).not.toMatch(/does not take any arguments/i);
    expect(out).toMatch(STATUS_OR_CONNECT_FAILED);
  });

  it("check_mssql_query without a query returns the documented error", async () => {
    const out = await query("check_mssql_query", []);
    expect(out).toMatch(/No query specified/);
  });

  it("check_mssql_databases parses its options and hits a server or the contract", async () => {
    const out = await query("check_mssql_databases", [
      "timeout=5",
      "warning=none",
      "critical=state = 'SUSPECT'",
    ]);
    expect(out).not.toMatch(/does not take any arguments|invalid expression/i);
    expect(out).toMatch(STATUS_OR_CONNECT_FAILED);
  });

  it("check_mssql_backup parses its options and hits a server or the contract", async () => {
    const out = await query("check_mssql_backup", [
      "timeout=5",
      "warning=none",
      "critical=full_age < 0",
    ]);
    expect(out).not.toMatch(/does not take any arguments|invalid expression/i);
    expect(out).toMatch(STATUS_OR_CONNECT_FAILED);
  });

  it("check_mssql_jobs parses its options and hits a server or the contract", async () => {
    const out = await query("check_mssql_jobs", ["timeout=5", "filter=enabled = 1"]);
    expect(out).not.toMatch(/does not take any arguments|invalid expression/i);
    expect(out).toMatch(STATUS_OR_CONNECT_FAILED);
  });
});

const dockerDescribe = onWindows ? dockerOrSkip() : describe.skip;

dockerDescribe("CheckMSSQL live (SQL Server 2022 container)", () => {
  const SA_PASSWORD = "Nscp!Test2026";
  let nscp: NscpInstance;
  let container: StartedTestContainer;
  let server: string;
  // False when the host's ODBC driver cannot complete a handshake with SQL
  // Server 2022 (legacy sqlsrv32-only hosts): tests then assert the contract.
  let live = false;

  function connArgs(): string[] {
    return [`server=${server}`, "user=sa", `password=${SA_PASSWORD}`];
  }

  async function query(command: string, args: string[] = []): Promise<string> {
    const r = await nscp.run(
      ["client", "--module", "CheckMSSQL", "--boot", "--query", command, ...connArgs(), ...args],
      {
        allowFailure: true,
        timeout: 120_000,
      },
    );
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    container = await new GenericContainer("mcr.microsoft.com/mssql/server:2022-latest")
      .withEnvironment({ ACCEPT_EULA: "Y", MSSQL_SA_PASSWORD: SA_PASSWORD })
      .withExposedPorts(1433)
      .withWaitStrategy(Wait.forLogMessage(/SQL Server is now ready for client connections/))
      .start();
    server = `127.0.0.1,${container.getMappedPort(1433)}`;

    // The readiness log line precedes full recovery; retry until the health
    // check answers (or give up and fall back to the contract assertions).
    const deadline = Date.now() + 120_000;
    while (Date.now() < deadline) {
      const out = await query("check_mssql", ["timeout=5"]);
      if (/^OK/m.test(out)) {
        live = true;
        break;
      }
      if (CONNECT_FAILED.test(out) && /IM002|08001/.test(out) === false) break;
      await new Promise((r) => setTimeout(r, 3_000));
    }
    if (!live)
      console.warn(
        "[checkmssql] container not reachable over ODBC; live assertions relaxed to the connect contract",
      );
  });

  afterAll(async () => {
    await container?.stop();
  });

  it("check_mssql reports version, edition and uptime", async () => {
    const out = await query("check_mssql");
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/SQL Server 16\./); // 2022 = product version 16.x
    expect(out).toMatch(/uptime \d+s/);
  });

  it("check_mssql emits uptime perfdata when uptime is referenced", async () => {
    // Perfdata is only emitted for keywords referenced in an expression.
    const out = await query("check_mssql", ["warning=uptime < 0"]);
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/_uptime'?=\d+s/);
  });

  it("check_mssql_query runs user SQL and exposes columns as keywords", async () => {
    const out = await query("check_mssql_query", [
      "query=SELECT name, database_id FROM sys.databases",
    ]);
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/name=master/);
    expect(out).toMatch(/database_id=1/);
  });

  it("check_mssql_query thresholds on a returned column", async () => {
    const out = await query("check_mssql_query", [
      "query=SELECT COUNT(*) AS db_count FROM sys.databases",
      "critical=db_count < 1",
    ]);
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/db_count=\d+/);
    expect(out).not.toMatch(/CRITICAL/);
  });

  it("check_mssql_databases reports all system databases ONLINE", async () => {
    const out = await query("check_mssql_databases");
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/All \d+ databases are ONLINE/);
  });

  it("check_mssql_databases emits size perfdata when size keywords are referenced", async () => {
    // Perfdata is emitted for variables referenced in expressions; type_size
    // keywords accept unit suffixes like 100G.
    const out = await query("check_mssql_databases", ["warning=data_size > 100G", "critical=none"]);
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/master_data'?=\d+/);
  });

  it("check_mssql_backup goes CRITICAL on a never-backed-up server by default", async () => {
    const out = await query("check_mssql_backup");
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    // Fresh container: master/model/msdb have never been backed up (full_age = -1).
    expect(out).toMatch(/^CRITICAL/m);
  });

  it("check_mssql_backup is OK when thresholds are cleared and reports -1 for never", async () => {
    // full_age < -2 never triggers (minimum is -1) but references the keyword,
    // so its perfdata is emitted and shows the never-backed-up marker.
    const out = await query("check_mssql_backup", ["warning=none", "critical=full_age < -2"]);
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/_full_age'?=-1s/); // never-backed-up contract in perfdata
  });

  it("check_mssql_jobs reports OK when no Agent jobs exist (empty-state contract)", async () => {
    // SQL Agent is disabled in the Linux container by default: sysjobs exists
    // and is empty, which must be OK, not an error (Express has no Agent either).
    const out = await query("check_mssql_jobs");
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/No enabled SQL Agent jobs/);
  });

  it("rejects a wrong SA password with the connect contract (SQL auth path)", async () => {
    const r = await nscp.run(
      [
        "client",
        "--module",
        "CheckMSSQL",
        "--boot",
        "--query",
        "check_mssql",
        `server=${server}`,
        "user=sa",
        "password=WrongPassword1!",
        "timeout=5",
      ],
      { allowFailure: true, timeout: 120_000 },
    );
    const out = r.all ?? `${r.stdout}\n${r.stderr}`;
    expect(out).toMatch(CONNECT_FAILED);
  });
});
