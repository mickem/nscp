/**
 * The PythonScript scripting API, end to end.
 *
 * One fixture script (`tests/fixtures/python/api_fixture.py`) calls every API
 * docs/docs/extending/python.md promises, each behind a query this suite drives
 * over REST, so a binding that stops working fails here rather than in someone's
 * script. The fixtures are copied into a scratch `${scripts}` - the
 * pythonscript-metrics pattern - so nothing is written into the build.
 *
 * Coverage checklist (the API, and how it is exercised):
 *
 * | API                                         | Exercised by                                                     |
 * | ------------------------------------------- | ---------------------------------------------------------------- |
 * | Registry.simple_function                    | every py_* query; REST `k=v` tokens reach the handler verbatim   |
 * | status enum                                 | py_status, one case per state                                    |
 * | Registry.simple_cmdline                     | `nscp client --module PythonScript --exec py_cli_echo`           |
 * | Registry.simple_subscription                | check_and_forward onto the script's channel                      |
 * | Core.simple_submit                          | py_submit onto the REST result cache channel, read back          |
 * | Core.simple_query                           | py_nested, into this module and into CheckHelpers                |
 * | Core.simple_exec                            | py_exec into the script's own command-line handler               |
 * | Core.load_module / unload_module / reload   | py_module; CheckHelpers' queries come and go; self-unload refused |
 * | Core.expand_path                            | py_expand of `${scripts}`                                        |
 * | Registry.event / event_pb                   | a real-time cpu filter's events reach both handlers              |
 * | Registry.fetch_metrics / submit_metrics     | the script's own metric comes back to its submit handler         |
 * | Settings.*                                  | register_path/key on /api/v2/settings/descriptions, get/set/section |
 * | __main__ / init / shutdown, both aliases    | the lifecycle trace, `nscp py execute`, a reload                 |
 * | Error paths                                 | raise, None, wrong shape, short tuple, a script that does not parse |
 * | /api/v2/scripts/py, `nscp py` verbs         | PUT, GET, list, DELETE, traversal; add/list/show/delete/install  |
 *
 * Not here: the raw-protobuf `Registry.function` / `subscription` / `cmdline`,
 * `Core.query` / `submit` / `exec` and `Settings.query`. They need the
 * generated `*_pb2` modules and the `protobuf` Python package next to the
 * agent, which this suite does not assume; nothing covers them yet.
 */
import * as fs from "fs";
import * as os from "os";
import * as path from "path";

import request from "supertest";

import {
  CRITICAL,
  NscpInstance,
  OK,
  REST_URL,
  UNKNOWN,
  WARNING,
  describeWithModules,
  executeQuery,
  messageOf,
  onWindows,
  perfOf,
  pollQuery,
  putSettings,
  setupQueryNscp,
} from "@fixtures/index";

jest.setTimeout(300_000);

const FIXTURES = path.join(__dirname, "fixtures", "python");
const SYSTEM_PATH = onWindows ? "/settings/system/windows" : "/settings/system/unix";

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

/**
 * A scratch work dir whose `${scripts}/python` holds the named fixtures, and an
 * NscpInstance pointed at it.
 */
function fixtureInstance(
  prefix: string,
  fixtures: string[],
): { nscp: NscpInstance; scripts: string } {
  const workDir = fs.mkdtempSync(path.join(os.tmpdir(), prefix));
  const scripts = path.join(workDir, "scripts");
  fs.mkdirSync(path.join(scripts, "python"), { recursive: true });
  for (const f of fixtures) {
    fs.copyFileSync(path.join(FIXTURES, f), path.join(scripts, "python", f));
  }
  return { nscp: new NscpInstance({ workDir, pathOverrides: { scripts } }), scripts };
}

/** The lifecycle trace the fixture appends to under `${data-path}`. */
function lifecycle(nscp: NscpInstance): string[] {
  const file = path.join(nscp.workDir, "pyapi-lifecycle.log");
  if (!fs.existsSync(file)) return [];
  return fs
    .readFileSync(file, "utf8")
    .split(/\r?\n/)
    .filter((l) => l !== "");
}

/** Lines of the agent's own log (what `nscp test` prints) containing `text`. */
function logLines(nscp: NscpInstance, text: string): string[] {
  return nscp
    .capturedStdout()
    .split(/\r?\n/)
    .filter((l) => l.includes(text));
}

async function until(what: string, check: () => boolean, timeoutMs = 60_000): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (!check()) {
    if (Date.now() >= deadline) throw new Error(`timed out waiting for ${what}`);
    await sleep(100);
  }
}

/**
 * The scripts endpoints answer with plain text (a message, or the script
 * itself) under a JSON content type, so read the body raw.
 */
function rawText(res: request.Response, callback: (err: Error | null, body: string) => void): void {
  // What superagent hands a custom parser is the response stream.
  const stream = res as unknown as NodeJS.ReadableStream;
  let data = "";
  stream.on("data", (chunk: Buffer) => (data += chunk.toString()));
  stream.on("end", () => callback(null, data));
}

async function queryNames(key: string): Promise<string[]> {
  const res = await request(REST_URL)
    .get("/api/v2/queries")
    .set("Authorization", `Bearer ${key}`)
    .trustLocalhost(true)
    .expect(200);
  return (res.body as { name: string }[]).map((q) => q.name);
}

describeWithModules("PythonScript")("PythonScript API", () => {
  describe("loaded into a running agent, driven over REST", () => {
    let nscp: NscpInstance;
    let scripts: string;
    let key: string;

    /** How many times the core has started reloading PythonScript so far. */
    const reloads = () => logLines(nscp, "Reloading: PythonScript").length;

    /** Ask the script to reload its own module, and wait until the core has. */
    async function reloadPython(): Promise<void> {
      const before = reloads();
      const r = await executeQuery(key, "py_module", { action: "reload", name: "PythonScript" });
      expect(r.result).toBe(OK);
      // From inside a dispatched call the core defers the reload to its own
      // thread, so the answer arrives first and the reload after.
      await until("PythonScript to reload", () => reloads() > before);
      await until(
        "py_echo to come back",
        () => logLines(nscp, "pyapi fixture loaded as pyapi").length > before + 1,
      );
    }

    beforeAll(async () => {
      ({ nscp, scripts } = fixtureInstance("nscp-py-api-", [
        "api_fixture.py",
        "alias_probe.py",
        "broken_syntax.py",
      ]));
      key = await setupQueryNscp(nscp, "PythonScript", {
        "/modules": {
          PythonScript: "enabled",
          WEBServer: "enabled",
          CheckHelpers: "enabled",
          CheckSystem: "enabled",
        },
        "/settings/python/scripts": {
          pyapi: "api_fixture.py",
          ms1: "alias_probe.py",
          ms2: "alias_probe.py",
          broken: "broken_syntax.py",
        },
        "/settings/pyapi": { number: "42", flag: "true" },
        "/settings/core": { "metrics interval": "1s" },
        "/settings/WEB/server/results": { enabled: "true", channel: "PYRESULTS" },
        // An always-matching real-time filter whose events the script
        // subscribes to by name.
        [`${SYSTEM_PATH}/real-time/cpu`]: { py_rt: "load >= 0" },
        [`${SYSTEM_PATH}/real-time/cpu/py_rt`]: { destination: "events" },
      });
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    describe("Registry.simple_function", () => {
      it("passes REST arguments to the handler as k=v tokens", async () => {
        const r = await executeQuery(key, "py_echo", { a: "1", flag: "true", "spaced arg": "x y" });
        expect(r.result).toBe(OK);
        expect(messageOf(r)).toBe("args: a=1|flag=true|spaced arg=x y");
        expect(perfOf(r).count).toMatchObject({ value: 3, warning: 5, critical: 8 });
      });

      it.each([
        ["ok", OK],
        ["warning", WARNING],
        ["critical", CRITICAL],
        ["unknown", UNKNOWN],
      ])("maps status.%s to its Nagios code", async (name, code) => {
        const r = await executeQuery(key, "py_status", { status: name, value: "7" });
        expect(r.result).toBe(code);
        expect(messageOf(r)).toBe(`status: ${name}`);
        expect(perfOf(r).value).toMatchObject({ value: 7, warning: 5, critical: 8 });
      });

      it("lists the script's queries with their descriptions", async () => {
        const res = await request(REST_URL)
          .get("/api/v2/queries/py_echo")
          .set("Authorization", `Bearer ${key}`)
          .trustLocalhost(true)
          .expect(200);
        expect(res.body.name).toBe("py_echo");
        expect(res.body.description).toBe("Echo the arguments back");
      });
    });

    describe("error paths", () => {
      it("a handler that raises answers UNKNOWN, logs the exception, and the agent carries on", async () => {
        const r = await executeQuery(key, "py_raise");
        expect(r.result).toBe(UNKNOWN);
        expect(messageOf(r)).toBe("Exception in: py_raise");
        // The traceback goes to the log, not to the caller.
        await until(
          "the traceback in the log",
          () => logLines(nscp, "boom from py_raise").length > 0,
        );
        expect((await executeQuery(key, "py_echo", { still: "alive" })).result).toBe(OK);
      });

      it("a handler returning None answers UNKNOWN", async () => {
        const r = await executeQuery(key, "py_none");
        expect(r.result).toBe(UNKNOWN);
        expect(messageOf(r)).toBe("None");
      });

      it("a handler returning something that is not a tuple answers UNKNOWN", async () => {
        const r = await executeQuery(key, "py_bad_shape");
        expect(r.result).toBe(UNKNOWN);
        expect(messageOf(r)).toBe("Exception in: py_bad_shape");
      });

      it("a one-element tuple is a status with no message", async () => {
        const r = await executeQuery(key, "py_short");
        expect(r.result).toBe(WARNING);
        expect(messageOf(r)).toBe("");
      });

      it("a script that does not parse is logged and the others load anyway", async () => {
        expect(
          logLines(nscp, "Failed to load script:").some((l) => l.includes("broken_syntax.py")),
        ).toBe(true);
        const names = await queryNames(key);
        expect(names).not.toContain("py_never");
        expect(names).toEqual(
          expect.arrayContaining(["py_echo", "alias_probe_ms1", "alias_probe_ms2"]),
        );
      });
    });

    describe("init", () => {
      it("receives the module alias and the script's own alias, once per configured entry", async () => {
        const ms1 = await executeQuery(key, "alias_probe_ms1");
        const ms2 = await executeQuery(key, "alias_probe_ms2");
        expect(messageOf(ms1)).toBe("plugin_alias=python script_alias=ms1");
        expect(messageOf(ms2)).toBe("plugin_alias=python script_alias=ms2");
        expect(lifecycle(nscp)).toContain("init python pyapi");
      });
    });

    describe("Core", () => {
      it("simple_query runs another query of the same module", async () => {
        const r = await executeQuery(key, "py_nested", {
          target: "py_status",
          status: "critical",
          value: "9",
        });
        expect(r.result).toBe(CRITICAL);
        expect(messageOf(r)).toBe("nested py_status: status: critical");
        expect(perfOf(r).value).toMatchObject({ value: 9 });
      });

      it("simple_query runs a query of another module", async () => {
        const r = await executeQuery(key, "py_nested", {
          target: "check_warning",
          message: "from python",
        });
        expect(r.result).toBe(WARNING);
        expect(messageOf(r)).toBe("nested check_warning: from python");
      });

      it("simple_exec runs a command-line handler", async () => {
        const r = await executeQuery(key, "py_exec", {
          module: "PythonScript",
          command: "py_cli_echo",
          arg: "hello",
        });
        expect(r.result).toBe(OK);
        expect(messageOf(r)).toBe("exec py_cli_echo: code=0 lines=cli: hello");
      });

      it("simple_submit lands on the channel it names", async () => {
        const r = await executeQuery(key, "py_submit", {
          channel: "PYRESULTS",
          command: "py_submitted",
          status: "critical",
          message: "submitted from python",
        });
        expect(r.result).toBe(OK);
        expect(messageOf(r)).toMatch(/^submitted: True/);

        const res = await request(REST_URL)
          .get("/api/v2/results")
          .set("Authorization", `Bearer ${key}`)
          .trustLocalhost(true)
          .expect(200);
        const entry = (res.body as Record<string, unknown>[]).find(
          (e) => e.command === "py_submitted",
        );
        expect(entry).toMatchObject({
          channel: "PYRESULTS",
          status: 2,
          message: "submitted from python",
        });
      });

      it("expand_path expands the paths the harness overrode", async () => {
        const r = await executeQuery(key, "py_expand", { "${scripts}": "" });
        // Compared as paths: Windows may hand back either separator.
        expect(path.resolve(messageOf(r))).toBe(path.resolve(scripts));
      });

      it("unload_module and load_module take a module's queries away and back", async () => {
        const unload = await executeQuery(key, "py_module", {
          action: "unload",
          name: "CheckHelpers",
        });
        expect(messageOf(unload)).toBe("unload CheckHelpers: True");
        expect(await queryNames(key)).not.toContain("check_ok");

        const load = await executeQuery(key, "py_module", { action: "load", name: "CheckHelpers" });
        expect(messageOf(load)).toBe("load CheckHelpers: True");
        expect(await queryNames(key)).toContain("check_ok");
      });

      it("unload_module refuses to unload the module the script runs in", async () => {
        const r = await executeQuery(key, "py_module", { action: "unload", name: "PythonScript" });
        expect(messageOf(r)).toBe("unload PythonScript: False");
        expect(logLines(nscp, "Refusing to unload PythonScript").length).toBeGreaterThan(0);
        expect((await executeQuery(key, "py_echo")).result).toBe(OK);
      });
    });

    describe("Registry.simple_subscription", () => {
      it("receives what is submitted on its channel", async () => {
        const res = await request(REST_URL)
          .get("/api/v2/queries/check_and_forward/commands/execute")
          .query({
            command: "check_warning",
            channel: "PYCHAN",
            source: "relay-host",
            arguments: "message=via channel",
          })
          .set("Authorization", `Bearer ${key}`)
          .trustLocalhost(true)
          .expect(200);
        expect(res.body).toMatchObject({ result: OK });

        const seen = messageOf(await executeQuery(key, "py_seen", { submissions: "" }));
        // channel|source|command|code|message|perf
        expect(seen).toMatch(/^PYCHAN\|relay-host\|check_warning\|1\|via channel\|/m);
      });

      it("a handler that returns False fails the submission", async () => {
        const res = await request(REST_URL)
          .get("/api/v2/queries/check_and_forward/commands/execute")
          .query({ command: "check_ok", channel: "PYREJECT" })
          .set("Authorization", `Bearer ${key}`)
          .trustLocalhost(true)
          .expect(200);
        expect(res.body.result).not.toBe(OK);
      });
    });

    describe("Registry.event / event_pb", () => {
      it("delivers a real-time filter's events to both handlers", async () => {
        const events = await pollQuery(
          key,
          "py_seen",
          { events: "" },
          (q) => messageOf(q) !== "none",
        );
        expect(messageOf(events)).toMatch(/^system\.cpu:py_rt keys=[1-9]\d*$/m);

        // The protobuf variant gets the serialised message as bytes.
        const pb = await pollQuery(
          key,
          "py_seen",
          { event_pb: "" },
          (q) => messageOf(q) !== "none",
        );
        expect(messageOf(pb)).toMatch(/^system\.cpu:py_rt bytes [1-9]\d*$/m);
      });
    });

    describe("Registry.fetch_metrics / submit_metrics", () => {
      it("hands the script's own metric back to its submit handler", async () => {
        const r = await pollQuery(key, "py_seen", { metrics: "" }, (q) => messageOf(q) !== "none");
        expect(messageOf(r)).toMatch(/pyapi\.fetched=7(\.0+)?$/m);
      });
    });

    describe("Settings", () => {
      it("register_path and register_key describe the section on /api/v2/settings/descriptions", async () => {
        const res = await request(REST_URL)
          .get("/api/v2/settings/descriptions/settings/pyapi")
          .set("Authorization", `Bearer ${key}`)
          .trustLocalhost(true)
          .expect(200);
        const body = JSON.stringify(res.body);
        expect(body).toContain("Python API fixture");
        expect(body).toContain("What py_settings reports");
        // The registered default is documentation for the UI ...
        expect(body).toContain('"hello"');
      });

      it("get_string, get_int, get_bool and get_section read the live settings", async () => {
        // ... and not a value: an unset key reads as the default the caller
        // passes, which is what docs/extending/python.md says register_key is.
        const r = await executeQuery(key, "py_settings");
        expect(messageOf(r)).toMatch(
          /^greeting=unset scratch=unset int=42 bool=True keys=flag,number$/,
        );

        // A change through REST is what the script reads next, no reload.
        await putSettings(key, "/settings/pyapi", { greeting: "hi" });
        expect(messageOf(await executeQuery(key, "py_settings"))).toMatch(/^greeting=hi /);
      });

      it("set_string is visible to the next read", async () => {
        const r = await executeQuery(key, "py_settings", { set: "written" });
        expect(messageOf(r)).toMatch(/ scratch=written /);
        expect(messageOf(r)).toMatch(/keys=.*\bscratch\b/);
      });
    });

    describe("/api/v2/scripts/py", () => {
      const auth = () => ({ Authorization: `Bearer ${key}` });

      it("is listed as a runtime", async () => {
        const res = await request(REST_URL)
          .get("/api/v2/scripts")
          .set(auth())
          .trustLocalhost(true)
          .expect(200);
        expect(res.body).toEqual(
          expect.arrayContaining([expect.objectContaining({ name: "py", module: "PythonScript" })]),
        );
      });

      it("lists the script files", async () => {
        const res = await request(REST_URL)
          .get("/api/v2/scripts/py?all=true")
          .set(auth())
          .trustLocalhost(true)
          .expect(200);
        const files = (res.body as string[]).map((f) => f.replace(/\\/g, "/"));
        expect(files).toEqual(
          expect.arrayContaining(["python/api_fixture.py", "python/alias_probe.py"]),
        );
      });

      it("PUT stores and loads a script, GET reads it back, DELETE removes it", async () => {
        const source = fs.readFileSync(path.join(FIXTURES, "rest_added.py"), "utf8");
        await request(REST_URL)
          .put("/api/v2/scripts/py/rest_added.py")
          .set(auth())
          .set("Content-Type", "text/plain")
          .send(source)
          .buffer(true)
          .parse(rawText)
          .trustLocalhost(true)
          .expect(200);
        expect(fs.readFileSync(path.join(scripts, "python", "rest_added.py"), "utf8")).toBe(source);
        // Loaded at once, no reload needed.
        expect(messageOf(await executeQuery(key, "py_rest_added"))).toBe("rest added");

        const show = await request(REST_URL)
          .get("/api/v2/scripts/py/rest_added.py")
          .set(auth())
          .buffer(true)
          .parse(rawText)
          .trustLocalhost(true)
          .expect(200);
        expect(show.body).toBe(source);

        // Configured, so it survives a reload.
        await reloadPython();
        expect(messageOf(await executeQuery(key, "py_rest_added"))).toBe("rest added");

        const del = await request(REST_URL)
          .delete("/api/v2/scripts/py/rest_added.py")
          .set(auth())
          .buffer(true)
          .parse(rawText)
          .trustLocalhost(true)
          .expect(200);
        expect(del.body).toMatch(
          /^Deleted .*rest_added\.py and removed it from \/settings\/python\/scripts/,
        );
        expect(fs.existsSync(path.join(scripts, "python", "rest_added.py"))).toBe(false);

        // Gone from the configuration too, so the next reload does not look
        // for it, and its query goes with it.
        await reloadPython();
        expect(await queryNames(key)).not.toContain("py_rest_added");
        expect(logLines(nscp, "Failed to find script: rest_added")).toEqual([]);
      });

      it("GET of a script that does not exist is an error, not an empty script", async () => {
        const res = await request(REST_URL)
          .get("/api/v2/scripts/py/no_such_script.py")
          .set(auth())
          .buffer(true)
          .parse(rawText)
          .trustLocalhost(true);
        expect(res.status).toBe(500);
        expect(res.body).toMatch(/Script not found: no_such_script\.py/);
      });

      it.each(["..%2Fnsclient.ini", "python%2F..%2F..%2Fnsclient.ini", "%2Fetc%2Fpasswd"])(
        "refuses the path %s",
        async (name) => {
          for (const method of ["get", "put", "delete"] as const) {
            const res = await request(REST_URL)
              [method](`/api/v2/scripts/py/${name}`)
              .set(auth())
              .trustLocalhost(true);
            expect(res.status).toBe(400);
          }
          expect(fs.existsSync(nscp.settingsFile)).toBe(true);
        },
      );
    });

    describe("a reload", () => {
      it("runs shutdown, then init again, without duplicating any registration", async () => {
        const inits = () => lifecycle(nscp).filter((l) => l === "init python pyapi").length;
        const shutdowns = () => lifecycle(nscp).filter((l) => l === "shutdown pyapi").length;
        const [i0, s0] = [inits(), shutdowns()];

        await reloadPython();

        expect(shutdowns()).toBe(s0 + 1);
        expect(inits()).toBe(i0 + 1);
        const names = await queryNames(key);
        expect(names.filter((n) => n === "py_echo")).toHaveLength(1);
        expect((await executeQuery(key, "py_echo", { after: "reload" })).result).toBe(OK);
      });
    });
  });

  describe("from the command line", () => {
    let nscp: NscpInstance;
    let scripts: string;

    beforeAll(async () => {
      ({ nscp, scripts } = fixtureInstance("nscp-py-cli-", ["api_fixture.py", "alias_probe.py"]));
      await nscp.configure({
        "/modules": { PythonScript: "enabled" },
        "/settings/python/scripts": { pyapi: "api_fixture.py" },
      });
    });

    const py = (args: string[]) => nscp.run(["py", ...args], { allowFailure: true });

    it("a simple_cmdline handler answers `nscp client --exec`", async () => {
      const r = await nscp.run(
        ["client", "--module", "PythonScript", "--exec", "py_cli_echo", "a", "b"],
        {
          allowFailure: true,
        },
      );
      expect(r.stdout).toContain("cli: a b");
      expect(r.exitCode).toBe(0);
    });

    it("a simple_cmdline handler that raises fails the command", async () => {
      const r = await nscp.run(["client", "--module", "PythonScript", "--exec", "py_cli_fail"], {
        allowFailure: true,
      });
      expect(r.stdout).toContain("Exception in: py_cli_fail");
      expect(r.exitCode).not.toBe(0);
    });

    it("`nscp py execute` runs init, then __main__ with the arguments, then shutdown", async () => {
      fs.rmSync(path.join(nscp.workDir, "pyapi-lifecycle.log"), { force: true });
      const r = await py(["execute", "--script", "api_fixture.py", "install", "--root", "/tmp"]);
      expect(r.exitCode).toBe(0);
      // The configured copy loads with the module and is shut down with it;
      // the executed copy is a second instance with no aliases.
      const trace = lifecycle(nscp);
      const executed = trace.filter((l) => !l.endsWith("pyapi"));
      expect(executed).toEqual(["init  ", "main install --root /tmp", "shutdown "]);
    });

    it("`nscp py execute` names a script it cannot find", async () => {
      const r = await py(["execute", "--script", "no_such_script.py"]);
      expect(r.all).toContain("Script not found: no_such_script.py");
      expect(r.exitCode).not.toBe(0);
    });

    it("`nscp py list` lists the scripts folder, and --json as an array", async () => {
      const r = await py(["list"]);
      expect(r.stdout.replace(/\\/g, "/")).toMatch(/^python\/api_fixture\.py$/m);
      const j = await py(["list", "--json"]);
      const files = (JSON.parse(j.stdout.trim().split(/\r?\n/).pop() ?? "[]") as string[]).map(
        (f) => f.replace(/\\/g, "/"),
      );
      expect(files).toEqual(
        expect.arrayContaining(["python/api_fixture.py", "python/alias_probe.py"]),
      );
    });

    it("`nscp py add`, `show` and `delete` manage one script", async () => {
      fs.copyFileSync(
        path.join(FIXTURES, "rest_added.py"),
        path.join(scripts, "python", "cli_added.py"),
      );

      const add = await py(["add", "--script", "cli_added.py"]);
      expect(add.stdout).toContain("Added cli_added.py as cli_added.py");
      expect(fs.readFileSync(nscp.settingsFile, "utf8")).toMatch(
        /^cli_added\.py\s*=\s*cli_added\.py$/m,
      );

      const show = await py(["show", "--script", "cli_added.py"]);
      expect(show.stdout).toContain("py_rest_added");
      expect(show.exitCode).toBe(0);

      const del = await py(["delete", "--script", "cli_added.py"]);
      expect(del.stdout).toContain("Deleted");
      expect(del.exitCode).toBe(0);
      expect(fs.existsSync(path.join(scripts, "python", "cli_added.py"))).toBe(false);
      expect(fs.readFileSync(nscp.settingsFile, "utf8")).not.toMatch(/cli_added/);

      const again = await py(["show", "--script", "cli_added.py"]);
      expect(again.all).toContain("Script not found: cli_added.py");
      expect(again.exitCode).not.toBe(0);
    });

    it("`nscp py show` and `delete` stay inside the scripts folder", async () => {
      // nsclient.ini sits two levels above ${scripts}/python.
      for (const verb of ["show", "delete"]) {
        const r = await py([verb, "--script", "../../nsclient.ini"]);
        expect(r.all).toContain("Not allowed outside");
        expect(r.exitCode).not.toBe(0);
      }
      expect(fs.existsSync(nscp.settingsFile)).toBe(true);
    });

    it("`nscp py install` adds and removes configured scripts", async () => {
      const add = await py(["install", "--add", "alias_probe.py"]);
      expect(add.exitCode).toBe(0);
      expect(fs.readFileSync(nscp.settingsFile, "utf8")).toMatch(
        /^alias_probe\.py\s*=\s*alias_probe\.py$/m,
      );

      const dup = await py(["install", "--add", "alias_probe.py"]);
      expect(dup.all).toContain("Failed to add duplicate script: alias_probe.py");

      const missing = await py(["install", "--add", "no_such_script.py"]);
      expect(missing.all).toContain("Failed to find: no_such_script.py");

      const remove = await py(["install", "--remove", "alias_probe.py"]);
      expect(remove.exitCode).toBe(0);
      expect(fs.readFileSync(nscp.settingsFile, "utf8")).not.toMatch(/^alias_probe\.py\s*=/m);
    });
  });
});
