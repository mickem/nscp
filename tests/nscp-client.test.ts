/**
 * Agent-to-agent checking: NSClient++ asking another NSClient++ to run a check.
 *
 * This path had no test at all, and did not work. It used to POST a serialized
 * QueryRequestMessage to the remote's /query.pb route — a route that let the
 * caller author the header the remote's permission layer read its identity
 * from, and which this client never spoke correctly anyway (it passed the
 * serialized message as the HTTP request *target*, so the remote saw a
 * malformed request line and no body). The route is gone; the client now uses
 * the versioned API every other client uses:
 *
 *   GET /api/v2/queries/<command>/commands/execute?<args>
 *   Accept: text/plain      -> "message|perfdata", Nagios result in the status
 *   password: <password>    -> the same header Icinga's check_nscp_api sends
 *
 * One real agent serves the REST API; a second, one-shot `nscp client` process
 * is the caller. The exit code of that process is the Nagios status, which is
 * what makes the HTTP-status-to-result mapping observable from out here.
 */
import { NscpInstance, bundledLuaScript, generateCertChain } from "@fixtures/index";

jest.setTimeout(300_000);

const PASSWORD = "nscp-client-test-password";
const PORT = 8444;

describe("NSCP client (agent to agent)", () => {
  let server: NscpInstance;
  let client: NscpInstance;

  /**
   * Run one query through NSCPClient against the agent above and return both
   * the output and the exit code. The client-query path prints the raw
   * `message|perfdata` with no status word, so the code is how the verdict is
   * asserted (0 OK / 1 WARNING / 2 CRITICAL / 3 UNKNOWN).
   */
  async function remote(
    args: string[],
    password: string = PASSWORD,
  ): Promise<{ out: string; code: number }> {
    const r = await client.run(
      [
        "client",
        "--module",
        "NSCPClient",
        "--boot",
        "--query",
        "check_remote_nscp",
        `address=127.0.0.1:${PORT}`,
        `password=${password}`,
        ...args,
      ],
      { allowFailure: true },
    );
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  beforeAll(async () => {
    server = new NscpInstance();
    client = new NscpInstance();

    // The Linux build ships no default WEBServer certificate, so generate one
    // and point the server at it explicitly. The client's default verify mode
    // is `none`, which is what lets it talk to an agent's self-signed
    // certificate at all.
    const certs = generateCertChain({
      outDir: server.scratch("certs"),
      signed: { server: { commonName: "localhost", isServer: true } },
    });

    await server.configure({
      "/modules": {
        WEBServer: "enabled",
        CheckHelpers: "enabled",
        LUAScript: "enabled",
      },
      "/settings/default": {
        password: PASSWORD,
        "allowed hosts": "127.0.0.1,::1",
      },
      // The `password` header authenticates as the implicit `admin` user
      // (session_manager_interface::process_password_header), so that user has
      // to exist with this password and hold a role that permits
      // queries.execute. The agent seeds both on a fresh install, but a
      // sandbox built from a generated ini does not come up with a live
      // credential the header can validate against - every other REST suite
      // here declares them explicitly for the same reason.
      "/settings/WEB/server/roles": { full: "*" },
      "/settings/WEB/server/users/admin": {
        role: "full",
        password: PASSWORD,
      },
      "/settings/WEB/server": {
        port: String(PORT),
        certificate: certs.signed.server.certPath,
        "certificate key": certs.signed.server.keyPath,
      },
      // mock_query echoes the arguments it was given, which is how the
      // argument encoding is checked below.
      "/settings/lua/scripts": {
        mock: bundledLuaScript("mock"),
      },
    });

    server.start();
    await server.waitForPort(PORT, { timeoutMs: 60_000 });
  });

  afterAll(async () => {
    await server?.stop();
  });

  // --- the result of the remote check reaches the caller ---------------------

  it("returns OK for a check that passes on the remote agent", async () => {
    const { out, code } = await remote(["command=check_ok", "argument=message=all good"]);

    expect(code).toBe(0);
    expect(out).toContain("all good");
  });

  it("propagates WARNING from the remote agent", async () => {
    // 202 on the wire. Before this, every result came back the same way
    // because the status was never read at all.
    const { out, code } = await remote(["command=check_warning", "argument=message=getting warm"]);

    expect(code).toBe(1);
    expect(out).toContain("getting warm");
  });

  it("propagates CRITICAL from the remote agent", async () => {
    const { out, code } = await remote(["command=check_critical", "argument=message=on fire"]);

    expect(code).toBe(2);
    expect(out).toContain("on fire");
  });

  it("propagates UNKNOWN for a command the remote agent does not have", async () => {
    const { out, code } = await remote(["command=check_no_such_command_here"]);

    expect(code).toBe(3);
    expect(out).toMatch(/unknown command/i);
  });

  // --- what is sent ----------------------------------------------------------

  it("passes arguments through to the remote command", async () => {
    // mock_query answers "mock_query::<args>", so this asserts that the
    // arguments survived URL encoding on the way out and were decoded back
    // into the request payload on the remote.
    const { out, code } = await remote(["command=mock_query", "argument=a=b", "argument=c=d"]);

    expect(code).toBe(0);
    expect(out).toContain("mock_query::a=b,c=d");
  });

  it("carries an argument whose value needs escaping", async () => {
    // Spaces and '=' inside a value are what a filter or a threshold looks
    // like; they have to arrive intact rather than splitting the argument.
    const { out, code } = await remote(["command=check_ok", "argument=message=a b = c"]);

    expect(code).toBe(0);
    expect(out).toContain("a b = c");
  });

  it("returns the perf data the remote check produced", async () => {
    const { out } = await remote(["command=mock_query"]);

    // mock.lua attaches perf data; the transport carries it after the '|'.
    expect(out).toContain("|");
    expect(out).toMatch(/'?a label'?=/);
  });

  // --- authentication --------------------------------------------------------

  // These two assert UNKNOWN, which is also what a client that cannot talk to
  // anything returns - so on their own they pass just as happily when every
  // request is failing locally. They did exactly that when the HTTP response
  // parser threw on every real status line. So each one also pins that the
  // UNKNOWN was the remote refusing the credential, not this client falling
  // over before it got an answer.
  it("fails with UNKNOWN when the password is wrong", async () => {
    // The password is finally sent (it used to be read from the settings and
    // never put on the wire). A rejected credential must read as UNKNOWN, not
    // as a passing check.
    const { out, code } = await remote(["command=check_ok"], "definitely-not-the-password");

    expect(code).toBe(3);
    expect(out).not.toMatch(/bad lexical cast|socket error/i);
    expect(out).not.toContain("all good");
  });

  it("fails with UNKNOWN when no password is sent at all", async () => {
    const { out, code } = await remote(["command=check_ok"], "");

    expect(code).toBe(3);
    expect(out).not.toMatch(/bad lexical cast|socket error/i);
    expect(out).not.toContain("all good");
  });

  // --- the transport itself --------------------------------------------------

  it("reports UNKNOWN rather than OK when the remote agent is unreachable", async () => {
    const r = await client.run(
      [
        "client",
        "--module",
        "NSCPClient",
        "--boot",
        "--query",
        "check_remote_nscp",
        "address=127.0.0.1:1",
        "command=check_ok",
        "timeout=2",
        "retry=0",
      ],
      { allowFailure: true },
    );

    expect(r.exitCode).toBe(3);
  });
});
