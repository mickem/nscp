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

  it("check_mssql_blocking parses its options and hits a server or the contract", async () => {
    const out = await query("check_mssql_blocking", [
      "timeout=5",
      "warning=wait_time > 30s",
      "critical=wait_time > 5m",
    ]);
    expect(out).not.toMatch(/does not take any arguments|invalid expression/i);
    expect(out).toMatch(STATUS_OR_CONNECT_FAILED);
  });

  it("check_mssql_availability_groups parses its options and hits a server or the contract", async () => {
    const out = await query("check_mssql_availability_groups", [
      "timeout=5",
      "warning=redo_queue > 500M",
      "critical=log_send_queue > 1G",
    ]);
    expect(out).not.toMatch(/does not take any arguments|invalid expression/i);
    expect(out).toMatch(STATUS_OR_CONNECT_FAILED);
  });

  it("check_mssql_counters parses its options and hits a server or the contract", async () => {
    const out = await query("check_mssql_counters", [
      "timeout=5",
      "warning=hit_ratio < 90 or page_life_expectancy < 300",
    ]);
    expect(out).not.toMatch(/does not take any arguments|invalid expression/i);
    expect(out).toMatch(STATUS_OR_CONNECT_FAILED);
  });

  it("check_mssql_sessions parses its options and hits a server or the contract", async () => {
    const out = await query("check_mssql_sessions", [
      "timeout=5",
      "warning=sessions > 500",
      "critical=max_idle > 2h",
    ]);
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
      // IM002 (driver not found) is the one permanently fatal case: this host
      // has no usable ODBC driver, so retrying cannot help - give up now and
      // relax to the contract assertions. Everything else is transient while
      // the container recovers (08001/08S01 handshake errors, login failures,
      // login timeouts) and must keep being retried until the deadline;
      // bailing out on those silently downgraded the entire live block to
      // "expect a connect failure", which is how a real threshold bug shipped.
      if (/IM002/.test(out)) break;
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

  it("check_mssql_query skips leading row-count results in a batch", async () => {
    // Without SQLMoreResults the INSERT's row count is the "first result",
    // which has no columns, and the SELECT was silently dropped -> empty OK.
    const out = await query("check_mssql_query", [
      "query=CREATE TABLE #t(i int); INSERT INTO #t VALUES(1),(2); SELECT COUNT(*) AS n FROM #t;",
      "top-syntax=${status}: ${list}",
    ]);
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/n=2/);
  });

  it("check_mssql_query reports a statement with no result set instead of a silent OK", async () => {
    const out = await query("check_mssql_query", ["query=DECLARE @i int = 1;"]);
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/no result set/i);
    expect(out).not.toMatch(/^OK/m);
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

  it("check_mssql_backup matches the -1 never sentinel exactly", async () => {
    // Regression guard: full_age is a type_custom_int_1 keyword with a
    // duration converter, so a negative literal goes through parse_time. If
    // parse_time hands "-1" to stox_as_time_sec it throws and the converter
    // silently yields 0, turning this into `full_age = 0` - which never fires
    // and would report a never-backed-up database as OK.
    const out = await query("check_mssql_backup", ["warning=none", "critical=full_age = -1"]);
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/^CRITICAL/m);
    expect(out).not.toMatch(/Invalid time specification/);
  });

  it("check_mssql_backup ignores COPY_ONLY backups unless asked to include them", async () => {
    // A copy-only backup is not part of the scheduled restore chain, so it must
    // not make full_age look fresh and mask a failing backup job.
    await query("check_mssql_query", [
      "query=IF DB_ID('copydb') IS NULL CREATE DATABASE copydb;",
      "top-syntax=${status}",
    ]);
    await query("check_mssql_query", [
      "query=BACKUP DATABASE copydb TO DISK='/tmp/copydb_co.bak' WITH COPY_ONLY; SELECT 1 AS done;",
      "top-syntax=${status}",
    ]);

    const excluded = await query("check_mssql_backup", ["filter=name = 'copydb'"]);
    if (!live) return expect(excluded).toMatch(CONNECT_FAILED);
    // Still listed (the predicate is in the JOIN, not the WHERE) but counted as never.
    expect(excluded).toMatch(/^CRITICAL/m);
    expect(excluded).toMatch(/copydb/);
    expect(excluded).toMatch(/_full_age'?=-1s/);

    const included = await query("check_mssql_backup", [
      "filter=name = 'copydb'",
      "include-copy-only=true",
    ]);
    expect(included).toMatch(/^OK/m);
    expect(included).not.toMatch(/_full_age'?=-1s/);
  });

  it("check_mssql_jobs exposes is_running and keeps last_run_status for completed runs", async () => {
    const out = await query("check_mssql_jobs", [
      "warning=none",
      "critical=none",
      "filter=none",
      "detail-syntax=${name}: running=${is_running} status=${last_run_status}",
      "show-all",
    ]);
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    // The keyword must parse and render even when the Agent has no jobs.
    expect(out).not.toMatch(/Invalid expression|does not take any arguments/i);
  });

  it("check_mssql_jobs reports OK when no Agent jobs exist (empty-state contract)", async () => {
    // SQL Agent is disabled in the Linux container by default: sysjobs exists
    // and is empty, which must be OK, not an error (Express has no Agent either).
    const out = await query("check_mssql_jobs");
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/No enabled SQL Agent jobs/);
  });

  it("check_mssql_sessions reports the monitoring session itself", async () => {
    // The check's own connection is a user session, so at least one
    // (database, login) group always exists on a live server.
    const out = await query("check_mssql_sessions");
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/\/sa: \d+ sessions \(\d+ running\)/);
  });

  it("check_mssql_sessions emits session perfdata when referenced", async () => {
    const out = await query("check_mssql_sessions", ["warning=sessions > 500"]);
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/_sessions'?=\d+/);
  });

  it("check_mssql_sessions accepts time units on max_idle (REST-style token)", async () => {
    const out = await query("check_mssql_sessions", ["critical=max_idle > 12h"]);
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).not.toMatch(/Invalid time specification|invalid expression/i);
    expect(out).toMatch(/^OK/m);
  });

  it("check_mssql_blocking reports OK with no blocked sessions", async () => {
    const out = await query("check_mssql_blocking");
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/No blocked sessions/);
  });

  it("check_mssql_blocking accepts time units and the blocker keywords", async () => {
    const out = await query("check_mssql_blocking", [
      "warning=wait_time > 30s and blocker_idle = 1",
      "critical=wait_time > 5m",
      "detail-syntax=${session_id} blocked by ${root_blocker}",
    ]);
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).not.toMatch(/Invalid time specification|invalid expression/i);
    expect(out).toMatch(/^OK/m);
  });

  it("check_mssql_counters reports rates and emits perfdata by default", async () => {
    const out = await query("check_mssql_counters");
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/hit ratio/);
    // Default perf-config emits all counters without any threshold.
    expect(out).toMatch(/mssql_page_life_expectancy'?=\d+s/);
    expect(out).toMatch(/mssql_batch_requests'?=/);
    // The 1s sampling window ran: batch rate is a finite number, not -1.
    expect(out).not.toMatch(/mssql_batch_requests'?=-1/);
  });

  it("check_mssql_counters thresholds on the health keywords", async () => {
    // A fresh container serves everything from memory: hit_ratio ~100, PLE > 0.
    const out = await query("check_mssql_counters", [
      "warning=hit_ratio < 0",
      "critical=page_life_expectancy < 0 or deadlocks > 1000",
    ]);
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).not.toMatch(/invalid expression/i);
    expect(out).toMatch(/^OK/m);
  });

  it("check_mssql_availability_groups reports the documented no-AG contract", async () => {
    // The test container has no availability groups: the check must report
    // the documented OK + message, not an error or a skip.
    const out = await query("check_mssql_availability_groups");
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/No availability groups found/);
  });

  it("check_mssql_availability_groups honors empty-state for must-have-AG hosts", async () => {
    const out = await query("check_mssql_availability_groups", ["empty-state=critical"]);
    if (!live) return expect(out).toMatch(CONNECT_FAILED);
    expect(out).toMatch(/^CRITICAL/m);
    expect(out).toMatch(/No availability groups found/);
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
