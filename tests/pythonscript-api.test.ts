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
 * `Registry.subscription` is covered only for the lifetime of the message it
 * hands over, which needs no protobuf parsing.
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
  itIf,
  itOnUnix,
  messageOf,
  onWindows,
  perfOf,
  pollQuery,
  putSettings,
  QueryResult,
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
      const inits = () => lifecycle(nscp).filter((l) => l === "init python pyapi").length;
      const [reloadsBefore, initsBefore] = [reloads(), inits()];
      const r = await executeQuery(key, "py_module", { action: "reload", name: "PythonScript" });
      expect(r.result).toBe(OK);
      // From inside a dispatched call the core defers the reload to its own
      // thread, so the answer arrives first and the reload after. Wait for the
      // reload to start and then for this reload's own init() - the fixture
      // writes that line last thing before it returns - rather than comparing
      // counters that only line up while nothing else loads the script.
      await until("PythonScript to reload", () => reloads() > reloadsBefore);
      await until("the fixture to be initialised again", () => inits() > initsBefore);
    }

    beforeAll(async () => {
      ({ nscp, scripts } = fixtureInstance("nscp-py-api-", [
        "api_fixture.py",
        "alias_probe.py",
        "broken_syntax.py",
        "failing_handlers.py",
        "init_raises.py",
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
          failing: "failing_handlers.py",
          init_raises: "init_raises.py",
        },
        "/settings/pyapi": { number: "42", flag: "true", word: "abc" },
        // A caller who may read scripts but not add or delete them.
        "/settings/WEB/server/roles": {
          full: "*",
          scripts_ro:
            "public,login.get,scripts,scripts.lists.PythonScript,scripts.get.PythonScript",
        },
        "/settings/WEB/server/users/reader": { role: "scripts_ro", password: "reader-password" },
        "/settings/core": { "metrics interval": "1s" },
        "/settings/WEB/server/results": { enabled: "true", channel: "PYRESULTS" },
        // An always-matching real-time filter whose events the script
        // subscribes to by name.
        [`${SYSTEM_PATH}/real-time/cpu`]: { py_rt: "load >= 0", py_rt_raise: "load >= 0" },
        [`${SYSTEM_PATH}/real-time/cpu/py_rt`]: { destination: "events" },
        // The same events, to a handler in failing_handlers.py that raises.
        [`${SYSTEM_PATH}/real-time/cpu/py_rt_raise`]: { destination: "events" },
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

      it("simple_exec on the `any` target reports the module that ran it as a success", async () => {
        const r = await executeQuery(key, "py_exec", {
          module: "any",
          command: "py_cli_echo",
          arg: "hello",
        });
        expect(messageOf(r)).toBe("exec py_cli_echo: code=0 lines=cli: hello");
      });

      it.each(["*", "all"])(
        "simple_exec on the `%s` target runs the command wherever it is",
        async (module) => {
          const r = await executeQuery(key, "py_exec", {
            module,
            command: "py_cli_echo",
            arg: "hi",
          });
          expect(messageOf(r)).toBe("exec py_cli_echo: code=0 lines=cli: hi");
        },
      );

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

      it("unload_module does not mistake `python` for the module the script runs in", async () => {
        // A module loaded without an alias hands its scripts `python` as
        // plugin_alias, but that is not a name it has in the core, so a
        // script asking to unload `python` must not be refused as unloading
        // itself. The core then answers for the name - and it unloads by
        // module name only, so nothing is called `python` there.
        const r = await executeQuery(key, "py_module", { action: "unload", name: "python" });
        expect(messageOf(r)).toBe("unload python: False");
        expect(logLines(nscp, "Refusing to unload python")).toEqual([]);
        expect(logLines(nscp, "Module python was not found").length).toBeGreaterThan(0);
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

      it("source is empty when the submission names none", async () => {
        const res = await request(REST_URL)
          .get("/api/v2/queries/check_and_forward/commands/execute")
          .query({ command: "check_ok", channel: "PYCHAN", alias: "no_source_named" })
          .set("Authorization", `Bearer ${key}`)
          .trustLocalhost(true)
          .expect(200);
        expect(res.body).toMatchObject({ result: OK });
        const seen = messageOf(await executeQuery(key, "py_seen", { submissions: "" }));
        expect(seen).toMatch(/^PYCHAN\|\|check_ok\|0\|/m);
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

    describe("Registry.subscription", () => {
      it("a message the handler keeps is still readable after it returns", async () => {
        // The handler gets a memoryview. It used to view the agent's own
        // temporary copy of the request, so one kept past the return read
        // freed memory; it now views a bytes object it keeps alive itself.
        const r = await executeQuery(key, "py_submit", {
          channel: "PYRAW",
          message: "kept past the handler",
        });
        expect(messageOf(r)).toMatch(/^submitted: True/);
        // Churn the heap a little before reading the view back.
        for (let i = 0; i < 5; i++) await executeQuery(key, "py_echo", { filler: "x".repeat(256) });
        const kept = messageOf(await executeQuery(key, "py_seen", { raw_kept: "" }));
        expect(kept).toBe("memoryview marker=True");
      });
    });

    describe("negative paths", () => {
      // What a script gets back when the call it makes cannot succeed. Each
      // is an answer the script can act on, never an exception it did not
      // ask for, and the agent keeps serving afterwards.

      it("Core.simple_query of a command nobody registered answers UNKNOWN and names it", async () => {
        const r = await executeQuery(key, "py_nested", { target: "no_such_check" });
        expect(r.result).toBe(UNKNOWN);
        expect(messageOf(r)).toBe("nested no_such_check: Unknown command(s): no_such_check");
      });

      it.each([
        ["a module that is not loaded", "NoSuchModule", "x"],
        ["a module with no such command", "PythonScript", "no_such_cmd"],
        ["the target the docs used to suggest", "local", "py_cli_echo"],
      ])("Core.simple_exec on %s answers UNKNOWN with a reason", async (_what, module, command) => {
        const r = await executeQuery(key, "py_exec", { module, command });
        // py_exec reports the code and lines simple_exec returned.
        expect(messageOf(r)).toBe(
          `exec ${command}: code=3 lines=Failed to execute ${command} on ${module}`,
        );
      });

      it("Core.simple_submit on a channel nobody listens to returns False and why", async () => {
        const r = await executeQuery(key, "py_submit", { channel: "NOBODY_LISTENS" });
        expect(r.result).toBe(CRITICAL);
        expect(messageOf(r)).toBe("submitted: False Failed to submit message: NOBODY_LISTENS");
      });

      it.each(["load", "unload"])(
        "Core.%s_module of a module that does not exist returns False",
        async (action) => {
          const r = await executeQuery(key, "py_module", { action, name: "NoSuchModule" });
          expect(messageOf(r)).toBe(`${action} NoSuchModule: False`);
        },
      );

      it("typed Settings reads of a value of the wrong type, or of a missing section, behave as documented", async () => {
        // word = abc. get_int falls back to the default; get_bool reads any
        // value but true/1/yes as False and uses the default (True here) only
        // for an unset key - which docs/extending/python.md now says.
        const r = await executeQuery(key, "py_settings_bad");
        expect(messageOf(r)).toBe("int=-1 bool=False missing=default");
      });

      it("an init() that raises is logged, keeps what it registered first, and the other scripts load", async () => {
        expect(logLines(nscp, "boom from init").length).toBeGreaterThan(0);
        expect(messageOf(await executeQuery(key, "py_before_raise"))).toBe(
          "registered before the raise",
        );
        expect((await executeQuery(key, "py_echo")).result).toBe(OK);
      });

      it("a subscription handler that raises fails the submission and is logged", async () => {
        const res = await request(REST_URL)
          .get("/api/v2/queries/check_and_forward/commands/execute")
          .query({ command: "check_ok", channel: "PYRAISE" })
          .set("Authorization", `Bearer ${key}`)
          .trustLocalhost(true)
          .expect(200);
        expect(res.body.result).toBe(UNKNOWN);
        expect(messageOf(res.body)).toBe("Failed to submit to PYRAISE: Invalid response: PYRAISE");
        await until(
          "the traceback in the log",
          () => logLines(nscp, "boom from on_submission").length > 0,
        );
        // The script's other channel is unaffected.
        const ok = await request(REST_URL)
          .get("/api/v2/queries/check_and_forward/commands/execute")
          .query({ command: "check_ok", channel: "PYCHAN" })
          .set("Authorization", `Bearer ${key}`)
          .trustLocalhost(true)
          .expect(200);
        expect(ok.body.result).toBe(OK);
      });

      it("an event handler that raises is logged, and the other handlers keep getting events", async () => {
        await until(
          "a few of the event handler's tracebacks",
          () => logLines(nscp, "Exception in system.cpu:py_rt_raise").length >= 3,
        );
        // Each failure is logged with its own traceback and no other: the
        // buffer the traceback is read from used to keep every earlier one
        // too, so the log grew with the square of the failures. Read once, so
        // a failure logged in between cannot skew the two counts.
        const log = nscp.capturedStdout().split(/\r?\n/);
        const failures = log.filter((l) =>
          l.includes("Exception in system.cpu:py_rt_raise"),
        ).length;
        const tracebacks = log.filter((l) =>
          l.startsWith("RuntimeError: boom from on_event"),
        ).length;
        expect(tracebacks).toBe(failures);
        const count = async () =>
          messageOf(await executeQuery(key, "py_seen", { events: "" }))
            .split("\n")
            .filter((l) => l.startsWith("system.cpu:py_rt ")).length;
        const before = await count();
        const after = await pollQuery(
          key,
          "py_seen",
          { events: "" },
          (q) =>
            messageOf(q)
              .split("\n")
              .filter((l) => l.startsWith("system.cpu:py_rt ")).length > before,
        );
        expect(
          messageOf(after)
            .split("\n")
            .filter((l) => l.startsWith("system.cpu:py_rt ")).length,
        ).toBeGreaterThan(before);
      });

      it("a metrics handler that raises is logged, and the other scripts' metrics still flow", async () => {
        await until(
          "fetch_metrics' traceback",
          () => logLines(nscp, "boom from fetch_metrics").length > 0,
        );
        await until(
          "submit_metrics' traceback",
          () => logLines(nscp, "boom from submit_metrics").length > 0,
        );
        const r = await pollQuery(key, "py_seen", { metrics: "" }, (q) => messageOf(q) !== "none");
        expect(messageOf(r)).toMatch(/pyapi\.fetched=7(\.0+)?$/m);
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

    describe("event delivery", () => {
      it("never hands an event to a channel handler that shares its name", async () => {
        // The fixture subscribes to a channel named exactly like the event it
        // also listens for. Events must reach the event handlers only.
        await pollQuery(key, "py_seen", { event_counts: "" }, (q) =>
          /^pb=([3-9]|\d\d)/.test(messageOf(q)),
        );
        expect(messageOf(await executeQuery(key, "py_seen", { collisions: "" }))).toBe("none");
      });

      it("hands event_pb each message once, and event each record of it once", async () => {
        // A real-time CPU filter sends one message with a record per core.
        // The core used to deliver that message once per record, and each
        // delivery walked every record: N records arrived N² times.
        const parse = (q: QueryResult) => {
          const m = /^pb=(\d+) records=(\d+) min=(\d+) max=(\d+)$/.exec(messageOf(q));
          if (!m) throw new Error(`unexpected event_counts: ${messageOf(q)}`);
          return { pb: Number(m[1]), records: Number(m[2]), min: Number(m[3]), max: Number(m[4]) };
        };
        const q = await pollQuery(key, "py_seen", { event_counts: "" }, (r) => parse(r).pb >= 3);
        const c = parse(q);
        expect(c.records).toBeGreaterThan(0);
        // Read mid-message, the records not yet delivered lag by one.
        expect(c.max).toBeLessThanOrEqual(c.pb);
        expect(c.min).toBeGreaterThanOrEqual(c.pb - 1);
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
          /^greeting=unset scratch=unset int=42 bool=True keys=flag,number,word$/,
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

      it("listing the queries, or asking one for its help, never runs a script handler", async () => {
        // Both ask the core for the registered queries. The listing used to ask
        // for every command's parameters too, which the core collects by
        // running the command - so it ran every check on the box, each script
        // handler included, side effects and all.
        await request(REST_URL)
          .get("/api/v2/scripts/py")
          .set(auth())
          .trustLocalhost(true)
          .expect(200);
        const help = await request(REST_URL)
          .get("/api/v2/queries/py_calls/help")
          .set(auth())
          .trustLocalhost(true);
        expect(help.status).toBe(200);
        expect(messageOf(await executeQuery(key, "py_calls"))).toBe("calls=1");
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

      it("a caller without the add and delete grants can read a script but not change it", async () => {
        const login = await request(REST_URL)
          .get("/api/v2/login")
          .auth("reader", "reader-password")
          .trustLocalhost(true)
          .expect(200);
        const reader = { Authorization: `Bearer ${login.body.key as string}` };
        const file = path.join(scripts, "python", "api_fixture.py");
        const before = fs.readFileSync(file, "utf8");

        const show = await request(REST_URL)
          .get("/api/v2/scripts/py/api_fixture.py")
          .set(reader)
          .buffer(true)
          .parse(rawText)
          .trustLocalhost(true)
          .expect(200);
        expect(show.body).toBe(before);

        await request(REST_URL)
          .put("/api/v2/scripts/py/api_fixture.py")
          .set(reader)
          .set("Content-Type", "text/plain")
          .send("raise SystemExit('replaced')\n")
          .trustLocalhost(true)
          .expect(403);
        await request(REST_URL)
          .delete("/api/v2/scripts/py/api_fixture.py")
          .set(reader)
          .trustLocalhost(true)
          .expect(403);
        expect(fs.readFileSync(file, "utf8")).toBe(before);
      });

      itOnUnix(
        "GET does not follow a symlink out of the scripts folder, DELETE removes only the link",
        async () => {
          const secret = path.join(nscp.workDir, "rest-secret.txt");
          fs.writeFileSync(secret, "not a script\n");
          const link = path.join(scripts, "python", "rest_leak.py");
          fs.symlinkSync(secret, link);
          try {
            const show = await request(REST_URL)
              .get("/api/v2/scripts/py/rest_leak.py")
              .set(auth())
              .buffer(true)
              .parse(rawText)
              .trustLocalhost(true);
            expect(show.status).toBe(500);
            expect(show.body).toContain("Not allowed outside");
            expect(show.body).not.toContain("not a script");

            // Deleting the link is safe whatever it points at: the link goes,
            // the file outside stays.
            await request(REST_URL)
              .delete("/api/v2/scripts/py/rest_leak.py")
              .set(auth())
              .buffer(true)
              .parse(rawText)
              .trustLocalhost(true)
              .expect(200);
            expect(fs.existsSync(link)).toBe(false);
            expect(fs.readFileSync(secret, "utf8")).toBe("not a script\n");
          } finally {
            fs.rmSync(link, { force: true });
          }
        },
      );

      it("an uploaded script that does not parse is logged and the module keeps serving", async () => {
        await request(REST_URL)
          .put("/api/v2/scripts/py/rest_broken.py")
          .set(auth())
          .set("Content-Type", "text/plain")
          .send("def init(plugin_id, plugin_alias, script_alias)\n    pass\n")
          .buffer(true)
          .parse(rawText)
          .trustLocalhost(true);
        await until("the parse error in the log", () =>
          logLines(nscp, "Failed to load script:").some((l) => l.includes("rest_broken.py")),
        );
        expect((await executeQuery(key, "py_echo")).result).toBe(OK);
        // Leave nothing behind for the reload test below.
        await request(REST_URL)
          .delete("/api/v2/scripts/py/rest_broken.py")
          .buffer(true)
          .parse(rawText)
          .set(auth())
          .trustLocalhost(true)
          .expect(200);
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

  describe("scripts/python/sample.py, loaded unchanged", () => {
    // The sample the docs point at, so a sample that stops working fails here.
    const SAMPLE = path.join(__dirname, "..", "scripts", "python", "sample.py");
    let nscp: NscpInstance;
    let key: string;

    beforeAll(async () => {
      const workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-py-sample-"));
      const scripts = path.join(workDir, "scripts");
      fs.mkdirSync(path.join(scripts, "python"), { recursive: true });
      fs.copyFileSync(SAMPLE, path.join(scripts, "python", "sample.py"));
      nscp = new NscpInstance({ workDir, pathOverrides: { scripts } });
      key = await setupQueryNscp(nscp, "PythonScript", {
        "/modules": { PythonScript: "enabled", WEBServer: "enabled" },
        "/settings/python/scripts": { sample: "sample.py" },
        "/settings/core": { "metrics interval": "1s" },
      });
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("breaks, checks, fixes and saves the world", async () => {
      expect(messageOf(await executeQuery(key, "check_world"))).toBe("The world is fine!");

      expect((await executeQuery(key, "break_world")).result).toBe(OK);
      const broken = await executeQuery(key, "check_world");
      expect(broken.result).toBe(CRITICAL);
      expect(messageOf(broken)).toBe("My god its full of stars: bad");

      const saved = await executeQuery(key, "save_world");
      expect(messageOf(saved)).toBe("The world is saved: bad");
      await until("the saved world in the settings file", () =>
        /^world\s*=\s*bad$/m.test(fs.readFileSync(nscp.settingsFile, "utf8")),
      );

      expect(messageOf(await executeQuery(key, "fix_world"))).toBe("Wicked! Safe!");
      expect((await executeQuery(key, "check_world")).result).toBe(OK);
    });

    it("show_metrics turns on logging every metric the agent collects", async () => {
      const on = await executeQuery(key, "show_metrics", { true: "" });
      expect(messageOf(on)).toBe("Metrics displayed enabled");
      await until("a metric in the log", () => logLines(nscp, "Got metrics: ").length > 0);
      expect(logLines(nscp, "Got metrics: ").some((l) => l.includes("number.of.times"))).toBe(true);
      const off = await executeQuery(key, "show_metrics", { false: "" });
      expect(messageOf(off)).toBe("Metrics displayed disabled");
    });

    it("its world_help command answers `nscp client --exec`", async () => {
      const r = await nscp.run(["client", "--module", "PythonScript", "--exec", "world_help"], {
        allowFailure: true,
      });
      expect(r.stdout).toContain("Need help? Sorry, Im not help full my friend...");
    });
  });

  describe("from the command line", () => {
    let nscp: NscpInstance;
    let scripts: string;

    beforeAll(async () => {
      // "lib" in the path: on a Linux package ${scripts} is
      // /usr/lib/nsclient/scripts, and `list` used to drop every file whose
      // path contained "lib" anywhere - which was all of them.
      ({ nscp, scripts } = fixtureInstance("nscp-py-cli-lib-", [
        "api_fixture.py",
        "alias_probe.py",
        "failing_handlers.py",
      ]));
      await nscp.configure({
        "/modules": { PythonScript: "enabled" },
        "/settings/python/scripts": { pyapi: "api_fixture.py", failing: "failing_handlers.py" },
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

    it.each([
      ["returns None", "py_cli_none", "None"],
      [
        "returns something that is not a tuple",
        "py_cli_bad_shape",
        "Exception in: py_cli_bad_shape",
      ],
    ])("a simple_cmdline handler that %s fails the command", async (_what, command, text) => {
      const r = await nscp.run(["client", "--module", "PythonScript", "--exec", command], {
        allowFailure: true,
      });
      expect(r.stdout).toContain(text);
      expect(r.exitCode).not.toBe(0);
    });

    it("`--exec` of a command no script registered names it and fails", async () => {
      const r = await nscp.run(["client", "--module", "PythonScript", "--exec", "no_such_cmd"], {
        allowFailure: true,
      });
      expect(r.all).toContain("Command not found: no_such_cmd");
      expect(r.exitCode).not.toBe(0);
    });

    it("`nscp py add` refuses a script that does not exist, and changes nothing", async () => {
      const before = fs.readFileSync(nscp.settingsFile, "utf8");
      const r = await py(["add", "--script", "no_such_script.py"]);
      expect(r.all).toContain("Script not found");
      expect(r.exitCode).not.toBe(0);
      expect(fs.readFileSync(nscp.settingsFile, "utf8")).toBe(before);
    });

    it("`nscp py add --import` will not overwrite a script without --replace", async () => {
      const source = path.join(nscp.workDir, "import_me.py");
      fs.copyFileSync(path.join(FIXTURES, "rest_added.py"), source);
      const first = await py([
        "add",
        "--script",
        "import_me.py",
        "--import",
        source,
        "--no-config",
      ]);
      expect(first.exitCode).toBe(0);

      fs.writeFileSync(source, "# a newer copy\n");
      const again = await py([
        "add",
        "--script",
        "import_me.py",
        "--import",
        source,
        "--no-config",
      ]);
      expect(again.all).toContain("Script already exists, specify --replace to replace it");
      expect(again.exitCode).not.toBe(0);
      expect(fs.readFileSync(path.join(scripts, "python", "import_me.py"), "utf8")).not.toContain(
        "a newer copy",
      );

      const replaced = await py([
        "add",
        "--script",
        "import_me.py",
        "--import",
        source,
        "--replace",
        "--no-config",
      ]);
      expect(replaced.exitCode).toBe(0);
      expect(fs.readFileSync(path.join(scripts, "python", "import_me.py"), "utf8")).toContain(
        "a newer copy",
      );
      fs.rmSync(path.join(scripts, "python", "import_me.py"));
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

    it("`nscp py list` leaves out the helpers in lib unless --include-lib", async () => {
      fs.mkdirSync(path.join(scripts, "python", "lib"), { recursive: true });
      fs.writeFileSync(path.join(scripts, "python", "lib", "a_helper.py"), "# helper\n");
      try {
        const plain = (await py(["list"])).stdout.replace(/\\/g, "/");
        expect(plain).toMatch(/^python\/api_fixture\.py$/m);
        expect(plain).not.toContain("a_helper.py");
        const all = (await py(["list", "--include-lib"])).stdout.replace(/\\/g, "/");
        expect(all).toMatch(/^python\/lib\/a_helper\.py$/m);
      } finally {
        fs.rmSync(path.join(scripts, "python", "lib"), { recursive: true, force: true });
      }
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

    itOnUnix(
      "`nscp py show` does not follow a symlink out of the scripts folder, `delete` removes only the link",
      async () => {
        const outside = path.join(nscp.workDir, "outside");
        fs.mkdirSync(outside, { recursive: true });
        const secret = path.join(outside, "secret.py");
        fs.writeFileSync(secret, "# not yours\n");
        // A link to a file, and a linked folder with the file inside it.
        fs.symlinkSync(secret, path.join(scripts, "python", "leak.py"));
        fs.symlinkSync(outside, path.join(scripts, "python", "linked"));
        try {
          // Through the linked folder, nothing is reachable at all.
          for (const verb of ["show", "delete"]) {
            const r = await py([verb, "--script", "linked/secret.py"]);
            expect(r.all).toContain("Not allowed outside");
            expect(r.exitCode).not.toBe(0);
          }
          // The link to a file outside cannot be read, but it can be deleted:
          // that removes the link and never what it points at.
          const show = await py(["show", "--script", "leak.py"]);
          expect(show.all).toContain("Not allowed outside");
          expect(show.all).not.toContain("# not yours");
          expect(show.exitCode).not.toBe(0);
          const del = await py(["delete", "--script", "leak.py"]);
          expect(del.exitCode).toBe(0);
          expect(fs.existsSync(path.join(scripts, "python", "leak.py"))).toBe(false);
          expect(fs.readFileSync(secret, "utf8")).toBe("# not yours\n");
        } finally {
          fs.rmSync(path.join(scripts, "python", "leak.py"), { force: true });
          fs.rmSync(path.join(scripts, "python", "linked"), { force: true });
        }
      },
    );

    itOnUnix(
      "`nscp py delete` of a symlink inside the folder removes the link, not its target",
      async () => {
        const target = path.join(scripts, "python", "real_target.py");
        fs.writeFileSync(target, "# stays\n");
        const link = path.join(scripts, "python", "inner_link.py");
        fs.symlinkSync(target, link);
        const r = await py(["delete", "--script", "inner_link.py"]);
        expect(r.exitCode).toBe(0);
        expect(fs.existsSync(link)).toBe(false);
        expect(fs.readFileSync(target, "utf8")).toBe("# stays\n");
        fs.rmSync(target);
      },
    );

    itOnUnix("`nscp py delete` removes a dangling symlink and a symlink to a folder", async () => {
      const dangling = path.join(scripts, "python", "dangling.py");
      fs.symlinkSync(path.join(nscp.workDir, "no-such-file.py"), dangling);
      const folder = path.join(nscp.workDir, "a-folder");
      fs.mkdirSync(folder, { recursive: true });
      fs.writeFileSync(path.join(folder, "inside.py"), "# stays\n");
      const folderLink = path.join(scripts, "python", "folder_link");
      fs.symlinkSync(folder, folderLink);

      for (const [name, link] of [
        ["dangling.py", dangling],
        ["folder_link", folderLink],
      ]) {
        const r = await py(["delete", "--script", name]);
        expect(r.exitCode).toBe(0);
        expect(fs.lstatSync(link, { throwIfNoEntry: false })).toBeUndefined();
      }
      expect(fs.readFileSync(path.join(folder, "inside.py"), "utf8")).toBe("# stays\n");
    });

    it("`nscp py delete` removes every entry that loads the script, however it is written", async () => {
      const file = path.join(scripts, "python", "many_entries.py");
      fs.copyFileSync(path.join(FIXTURES, "rest_added.py"), file);
      // A bare `file =` entry, an absolute path, and a Windows-style relative
      // one - next to one for another script that must survive.
      const entries = [
        "many_entries.py =",
        `by_absolute_path = ${file}`,
        "by_backslash = python\\many_entries.py",
        "unrelated = api_fixture.py",
      ];
      const ini = fs.readFileSync(nscp.settingsFile, "utf8");
      const header = "[/settings/python/scripts]";
      expect(ini).toContain(header);
      fs.writeFileSync(nscp.settingsFile, ini.replace(header, [header, ...entries].join("\n")));

      const r = await py(["delete", "--script", "many_entries.py"]);
      expect(r.stdout).toContain("and removed it from /settings/python/scripts");
      expect(r.exitCode).toBe(0);
      const after = fs.readFileSync(nscp.settingsFile, "utf8");
      expect(after).not.toMatch(/many_entries/);
      expect(after).not.toMatch(/by_absolute_path|by_backslash/);
      expect(after).toMatch(/^unrelated\s*=\s*api_fixture\.py$/m);
      expect(after).toMatch(/^pyapi\s*=\s*api_fixture\.py$/m);
    });

    // Root reads any file regardless of its mode, so this needs another user.
    itIf(!onWindows && process.getuid?.() !== 0)(
      "`nscp py show` of a script it cannot read is an error, not an empty script",
      async () => {
        const file = path.join(scripts, "python", "unreadable.py");
        fs.writeFileSync(file, "# secret\n");
        fs.chmodSync(file, 0o000);
        try {
          const r = await py(["show", "--script", "unreadable.py"]);
          expect(r.all).toContain("Failed to read");
          expect(r.exitCode).not.toBe(0);
        } finally {
          fs.chmodSync(file, 0o600);
          fs.rmSync(file);
        }
      },
    );

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
