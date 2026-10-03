/**
 * The LUAScript scripting API, end to end.
 *
 * One fixture script (`tests/fixtures/lua/api_fixture.lua`) calls every API
 * docs/docs/extending/lua.md promises, each behind a query this suite drives
 * over REST, so a binding that stops working fails here rather than in someone's
 * script. The fixtures are copied into a scratch `${scripts}` - the
 * pythonscript-api pattern - so nothing is written into the build.
 *
 * Coverage checklist (the API, and how it is exercised):
 *
 * | API                                          | Exercised by                                                      |
 * | -------------------------------------------- | ----------------------------------------------------------------- |
 * | Registry:simple_query / simple_function      | every lua_* query; REST `k=v` tokens reach the handler verbatim   |
 * | status strings and integer codes             | lua_status, one case per state and the integer form               |
 * | Registry:simple_cmdline                      | `nscp client --module LUAScript --exec`, and Core:simple_exec      |
 * | Registry:simple_subscription                 | check_and_forward onto the script's channels                      |
 * | Registry:query / cmdline / subscription      | raw/unsupported: an error the script can catch, naming the call   |
 * | Core:simple_query                            | lua_nested, into this module and into CheckHelpers                |
 * | Core:create_pb_query / query                 | lua_raw_query; the response is measured, not decoded              |
 * | Core:simple_exec                             | lua_exec into the script's own command-line handler               |
 * | Core:simple_submit                           | lua_submit onto the REST result cache channel, read back          |
 * | Core:reload                                  | lua_reload; the top-level code and on_start run again, once       |
 * | Core:log, nscp.info / print / error          | lua_log; the lines reach the agent's log at their level           |
 * | Core:exec / submit                           | unsupported: an error the script can catch, naming the call       |
 * | Core:query_target / query_forward            | nrpe-loopback.test.ts (they need an NRPE client and server)       |
 * | nscp.sleep / nscp.getSetting                 | lua_sleep, lua_settings_bad                                       |
 * | Settings:*                                   | register_path/key on /api/v2/settings/descriptions, get/set/section/save |
 * | top-level code / on_start / main             | the lifecycle trace, `nscp lua execute`, a reload                 |
 * | Error paths                                  | error(), no return, a bad status, a short return, scripts that fail to load or start |
 * | /api/v2/scripts/lua, `nscp lua` verbs        | PUT, GET, list, DELETE, traversal; add/list/show/delete/install    |
 * | The complete example in lua.md               | loaded unchanged, every handler it registers driven once          |
 *
 * Not here: the raw-protobuf `Registry:query` handler. Building its response
 * needs the Lua protobuf bindings, which no build ships (FindPROTOC_GEN_LUA
 * never reports them found), so a script cannot construct one.
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
  putSettings,
  setupQueryNscp,
} from "@fixtures/index";

jest.setTimeout(300_000);

const FIXTURES = path.join(__dirname, "fixtures", "lua");
const LUA_MD = path.join(__dirname, "..", "docs", "docs", "extending", "lua.md");

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

/**
 * A scratch work dir whose `${scripts}/lua` holds the named fixtures, and an
 * NscpInstance pointed at it.
 */
function fixtureInstance(
  prefix: string,
  fixtures: string[],
): { nscp: NscpInstance; scripts: string; trace: string } {
  const workDir = fs.mkdtempSync(path.join(os.tmpdir(), prefix));
  const scripts = path.join(workDir, "scripts");
  fs.mkdirSync(path.join(scripts, "lua"), { recursive: true });
  for (const f of fixtures) {
    fs.copyFileSync(path.join(FIXTURES, f), path.join(scripts, "lua", f));
  }
  return {
    nscp: new NscpInstance({ workDir, pathOverrides: { scripts } }),
    scripts,
    trace: path.join(workDir, "luaapi-lifecycle.log"),
  };
}

/** The lifecycle trace the fixture appends to. */
function lifecycle(trace: string): string[] {
  if (!fs.existsSync(trace)) return [];
  return fs
    .readFileSync(trace, "utf8")
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

/** The body of the one ```lua block under `## A complete example` in lua.md. */
function completeExample(): string {
  const md = fs.readFileSync(LUA_MD, "utf8");
  const section = md.split(/^## A complete example$/m)[1];
  if (!section) throw new Error("lua.md has no `## A complete example` section");
  const m = /^```lua\r?\n([\s\S]*?)^```$/m.exec(section);
  if (!m) throw new Error("the complete example in lua.md has no ```lua block");
  return m[1];
}

async function checkAndForward(
  key: string,
  query: Record<string, string>,
): Promise<{ result: number; lines?: { message: string }[] }> {
  const res = await request(REST_URL)
    .get("/api/v2/queries/check_and_forward/commands/execute")
    .query(query)
    .set("Authorization", `Bearer ${key}`)
    .trustLocalhost(true)
    .expect(200);
  return res.body;
}

describeWithModules("LUAScript")("LUAScript API", () => {
  describe("loaded into a running agent, driven over REST", () => {
    let nscp: NscpInstance;
    let scripts: string;
    let trace: string;
    let key: string;

    const loads = () => lifecycle(trace).filter((l) => l === "load").length;
    const starts = () => lifecycle(trace).filter((l) => l === "start").length;

    /** Ask the script to reload its own module, and wait until it has. */
    async function reloadLua(): Promise<void> {
      const [loadsBefore, startsBefore] = [loads(), starts()];
      const r = await executeQuery(key, "lua_reload", { name: "LUAScript" });
      expect(messageOf(r)).toBe("reload LUAScript requested");
      // From inside a dispatched call the core defers the reload to its own
      // thread, so the answer arrives first and the reload after. The fixture
      // writes `load` from its top-level code and `start` from on_start.
      await until("the fixture to be loaded again", () => loads() > loadsBefore);
      await until("the fixture to be started again", () => starts() > startsBefore);
    }

    beforeAll(async () => {
      ({ nscp, scripts, trace } = fixtureInstance("nscp-lua-api-", [
        "api_fixture.lua",
        "broken_syntax.lua",
        "load_raises.lua",
        "start_raises.lua",
      ]));
      key = await setupQueryNscp(nscp, "LUAScript", {
        "/modules": {
          LUAScript: "enabled",
          WEBServer: "enabled",
          CheckHelpers: "enabled",
        },
        "/settings/lua/scripts": {
          luaapi: "api_fixture.lua",
          broken: "broken_syntax.lua",
          load_raises: "load_raises.lua",
          start_raises: "start_raises.lua",
        },
        "/settings/luaapi": { trace, number: "42", flag: "true", word: "abc" },
        // A caller who may read scripts but not add or delete them.
        "/settings/WEB/server/roles": {
          full: "*",
          scripts_ro: "public,login.get,scripts,scripts.lists.LUAScript,scripts.get.LUAScript",
        },
        "/settings/WEB/server/users/reader": { role: "scripts_ro", password: "reader-password" },
        "/settings/WEB/server/results": { enabled: "true", channel: "LUARESULTS" },
      });
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    describe("Registry:simple_query", () => {
      it("passes REST arguments to the handler as k=v tokens", async () => {
        const r = await executeQuery(key, "lua_echo", {
          a: "1",
          flag: "true",
          "spaced arg": "x y",
        });
        expect(r.result).toBe(OK);
        expect(messageOf(r)).toBe("args: a=1|flag=true|spaced arg=x y");
        expect(perfOf(r).count).toMatchObject({ value: 3, warning: 5, critical: 8 });
      });

      it.each([
        ["ok", OK],
        ["warning", WARNING],
        ["critical", CRITICAL],
        ["unknown", UNKNOWN],
        ["0", OK],
        ["1", WARNING],
        ["2", CRITICAL],
        ["3", UNKNOWN],
      ])("maps the status %s to its Nagios code", async (name, code) => {
        const r = await executeQuery(key, "lua_status", { status: name, value: "7" });
        expect(r.result).toBe(code);
        expect(messageOf(r)).toBe(`status: ${name}`);
        expect(perfOf(r).value).toMatchObject({ value: 7, warning: 5, critical: 8 });
      });

      it("hands the handler the name it was registered under", async () => {
        const r = await executeQuery(key, "lua_command_name");
        expect(messageOf(r)).toBe("command: lua_command_name");
      });

      it("lists the script's queries with their descriptions, and a default for none", async () => {
        const described = await request(REST_URL)
          .get("/api/v2/queries/lua_echo")
          .set("Authorization", `Bearer ${key}`)
          .trustLocalhost(true)
          .expect(200);
        expect(described.body.description).toBe("Echo the arguments back");

        const undescribed = await request(REST_URL)
          .get("/api/v2/queries/lua_command_name")
          .set("Authorization", `Bearer ${key}`)
          .trustLocalhost(true)
          .expect(200);
        expect(undescribed.body.description).toBe("Lua script: lua_command_name");
      });
    });

    describe("error paths", () => {
      it("a handler that raises answers UNKNOWN with the error, and the agent carries on", async () => {
        const r = await executeQuery(key, "lua_raise");
        expect(r.result).toBe(UNKNOWN);
        expect(messageOf(r)).toMatch(
          /^Failed to handle command: lua_raise: .*api_fixture\.lua:\d+: boom from lua_raise$/,
        );
        expect((await executeQuery(key, "lua_echo", { still: "alive" })).result).toBe(OK);
      });

      it("a handler that returns nothing answers UNKNOWN", async () => {
        const r = await executeQuery(key, "lua_nothing");
        expect(r.result).toBe(UNKNOWN);
        expect(messageOf(r)).toBe(
          "Invalid return from lua_nothing: expected (code, message, perf)",
        );
      });

      it("a handler that returns only a status keeps it, with no message", async () => {
        const r = await executeQuery(key, "lua_short");
        expect(r.result).toBe(WARNING);
        expect(messageOf(r)).toBe("");
      });

      it("a status that is not one answers UNKNOWN", async () => {
        const r = await executeQuery(key, "lua_bad_code");
        expect(r.result).toBe(UNKNOWN);
      });

      it("a script that does not parse is logged and the others load anyway", async () => {
        expect(
          logLines(nscp, "Failed to load script").some((l) => l.includes("broken_syntax.lua")),
        ).toBe(true);
        const names = await queryNames(key);
        expect(names).not.toContain("lua_never");
        expect(names).toEqual(expect.arrayContaining(["lua_echo", "lua_start_raises"]));
      });

      it("a script whose top-level code raises is logged, keeps what it registered first, and the others load", async () => {
        expect(logLines(nscp, "boom from the top-level code").length).toBeGreaterThan(0);
        expect(messageOf(await executeQuery(key, "lua_before_raise"))).toBe(
          "registered before the raise",
        );
      });

      it("an on_start that raises is logged, and every script still starts", async () => {
        expect(logLines(nscp, "boom from on_start").length).toBeGreaterThan(0);
        expect(messageOf(await executeQuery(key, "lua_start_raises"))).toBe("still registered");
        expect(lifecycle(trace)).toContain("start");
      });

      it("a call with too few arguments raises an error naming its syntax", async () => {
        // Raised at the script's own line, which Lua puts in front.
        const r = await executeQuery(key, "lua_bad_syntax");
        expect(messageOf(r)).toMatch(
          /^ok=false err=.*api_fixture\.lua:\d+: Incorrect syntax: simple_query\(command, args\)$/,
        );
      });

      it.each([
        ["exec", "Core:exec"],
        ["submit", "Core:submit"],
        ["cmdline", "Registry:cmdline"],
        ["subscription", "Registry:subscription"],
      ])(
        "%s is not implemented, and says so as an error the script can catch",
        async (call, name) => {
          const r = await executeQuery(key, "lua_unsupported", { call });
          expect(messageOf(r)).toMatch(
            new RegExp(
              `^${call}: ok=false err=.*api_fixture\\.lua:\\d+: Unsupported API called: ${name}$`,
            ),
          );
        },
      );
    });

    describe("Core", () => {
      it("simple_query runs another query of the same module", async () => {
        const r = await executeQuery(key, "lua_nested", {
          target: "lua_status",
          status: "critical",
          value: "9",
        });
        expect(r.result).toBe(CRITICAL);
        expect(messageOf(r)).toBe("nested lua_status: status: critical");
        expect(perfOf(r).value).toMatchObject({ value: 9 });
      });

      it("simple_query runs a query of another module", async () => {
        const r = await executeQuery(key, "lua_nested", {
          target: "check_warning",
          message: "from lua",
        });
        expect(r.result).toBe(WARNING);
        expect(messageOf(r)).toBe("nested check_warning: from lua");
      });

      it("simple_query takes a single string as the argument list", async () => {
        const r = await executeQuery(key, "lua_nested_string", {
          target: "lua_echo",
          arg: "only=one",
        });
        expect(messageOf(r)).toBe("nested lua_echo: args: only=one");
      });

      it("simple_query of a command nobody registered answers UNKNOWN and names it", async () => {
        const r = await executeQuery(key, "lua_nested", { target: "no_such_check" });
        expect(r.result).toBe(UNKNOWN);
        expect(messageOf(r)).toBe("nested no_such_check: Unknown command(s): no_such_check");
      });

      it("create_pb_query and query run a query from a serialised request", async () => {
        const r = await executeQuery(key, "lua_raw_query", {
          target: "check_ok",
          message: "raw",
        });
        expect(messageOf(r)).toMatch(/^raw check_ok: ok=true request=[1-9]\d* response=[1-9]\d*$/);
      });

      it("simple_exec runs a command-line handler of this module", async () => {
        const r = await executeQuery(key, "lua_exec", {
          module: "LUAScript",
          command: "lua_cli_echo",
          arg: "hello",
        });
        expect(messageOf(r)).toBe("exec lua_cli_echo: code=ok lines=cli: hello");
      });

      it("simple_exec on the `any` target finds the handler", async () => {
        const r = await executeQuery(key, "lua_exec", {
          module: "any",
          command: "lua_cli_echo",
          arg: "hi",
        });
        expect(messageOf(r)).toBe("exec lua_cli_echo: code=ok lines=cli: hi");
      });

      it("simple_exec passes a non-OK code from the handler through", async () => {
        const r = await executeQuery(key, "lua_exec", {
          module: "LUAScript",
          command: "lua_cli_warn",
        });
        expect(messageOf(r)).toBe("exec lua_cli_warn: code=warning lines=cli warned");
      });

      it.each([
        ["a module that is not loaded", "NoSuchModule", "x"],
        ["a module with no such command", "LUAScript", "no_such_cmd"],
      ])("simple_exec on %s answers UNKNOWN with a reason", async (_what, module, command) => {
        const r = await executeQuery(key, "lua_exec", { module, command });
        expect(messageOf(r)).toBe(
          `exec ${command}: code=unknown lines=Failed to execute ${command} on ${module}`,
        );
      });

      it("simple_submit lands on the channel it names", async () => {
        const r = await executeQuery(key, "lua_submit", {
          channel: "LUARESULTS",
          command: "lua_submitted",
          status: "critical",
          message: "submitted from lua",
        });
        expect(r.result).toBe(OK);
        expect(messageOf(r)).toMatch(/^submitted: true/);

        const res = await request(REST_URL)
          .get("/api/v2/results")
          .set("Authorization", `Bearer ${key}`)
          .trustLocalhost(true)
          .expect(200);
        const entry = (res.body as Record<string, unknown>[]).find(
          (e) => e.command === "lua_submitted",
        );
        expect(entry).toMatchObject({
          channel: "LUARESULTS",
          status: 2,
          message: "submitted from lua",
        });
      });

      it("simple_submit on a channel nobody listens to returns false and why", async () => {
        const r = await executeQuery(key, "lua_submit", { channel: "NOBODY_LISTENS" });
        expect(r.result).toBe(CRITICAL);
        expect(messageOf(r)).toBe("submitted: false Failed to submit message: NOBODY_LISTENS");
      });

      it("log, nscp.info, nscp.print and nscp.error reach the agent's log", async () => {
        const r = await executeQuery(key, "lua_log", { level: "error", message: "lua-log-marker" });
        expect(messageOf(r)).toBe("logged");
        await until("the four lines in the log", () =>
          [
            "lua-log-marker",
            "nscp.info: lua-log-marker",
            "nscp.print: lua-log-marker",
            "nscp.error: lua-log-marker",
          ].every((t) => logLines(nscp, t).length > 0),
        );
        // Core:log is attributed to the script, not to the C++ wrapper: the
        // log prints the source location on the line after the message.
        const log = nscp.capturedStdout().split(/\r?\n/);
        const at = log.findIndex((l) => /^E\s+lua lua-log-marker$/.test(l));
        expect(at).toBeGreaterThanOrEqual(0);
        expect(log[at + 1]).toMatch(/api_fixture\.lua:\d+$/);
        // Each at its own level: L is info, E is error.
        expect(logLines(nscp, "nscp.info: lua-log-marker")[0]).toMatch(/^L\s/);
        expect(logLines(nscp, "nscp.print: lua-log-marker")[0]).toMatch(/^L\s/);
        expect(logLines(nscp, "nscp.error: lua-log-marker")[0]).toMatch(/^E\s/);
      });

      it("nscp.sleep sleeps for the milliseconds it is given", async () => {
        const started = Date.now();
        const r = await executeQuery(key, "lua_sleep", { ms: "300" });
        expect(messageOf(r)).toBe("slept 300");
        expect(Date.now() - started).toBeGreaterThanOrEqual(290);
      });
    });

    describe("Registry:simple_subscription", () => {
      it("receives what is submitted on its channel", async () => {
        const res = await checkAndForward(key, {
          command: "check_warning",
          channel: "LUACHAN",
          arguments: "message=via channel",
        });
        expect(res).toMatchObject({ result: OK });

        const seen = messageOf(await executeQuery(key, "lua_seen"));
        // channel|command|code|message=perf
        expect(seen).toMatch(/^LUACHAN\|check_warning\|warning\|via channel=/m);
      });

      it("a handler that returns false fails the submission", async () => {
        const res = await checkAndForward(key, { command: "check_ok", channel: "LUAREJECT" });
        expect(res.result).not.toBe(OK);
      });

      it("a handler that raises fails the submission, is logged, and the other channels carry on", async () => {
        const res = await checkAndForward(key, { command: "check_ok", channel: "LUARAISE" });
        expect(res.result).not.toBe(OK);
        await until("the error in the log", () => logLines(nscp, "boom from on_raise").length > 0);
        const ok = await checkAndForward(key, { command: "check_ok", channel: "LUACHAN" });
        expect(ok.result).toBe(OK);
      });
    });

    describe("Settings", () => {
      it("register_path and register_key describe the section on /api/v2/settings/descriptions", async () => {
        const res = await request(REST_URL)
          .get("/api/v2/settings/descriptions/settings/luaapi")
          .set("Authorization", `Bearer ${key}`)
          .trustLocalhost(true)
          .expect(200);
        const body = JSON.stringify(res.body);
        expect(body).toContain("Lua API fixture");
        expect(body).toContain("What lua_settings reports");
        expect(body).toContain('"hello"');
      });

      it("get_string, get_int, get_bool and get_section read the live settings", async () => {
        const r = await executeQuery(key, "lua_settings");
        expect(messageOf(r)).toMatch(
          /^greeting=unset scratch=unset int=42 bool=true counter=-1 switch=false keys=/,
        );
        expect(messageOf(r)).toMatch(/keys=flag,number,trace,word$/);

        // A change through REST is what the script reads next, no reload.
        await putSettings(key, "/settings/luaapi", { greeting: "hi" });
        expect(messageOf(await executeQuery(key, "lua_settings"))).toMatch(/^greeting=hi /);
      });

      it("set_string, set_int and set_bool are visible to the next read", async () => {
        const r = await executeQuery(key, "lua_settings", {
          set: "written",
          set_int: "17",
          set_bool: "true",
        });
        expect(messageOf(r)).toMatch(/ scratch=written /);
        expect(messageOf(r)).toMatch(/ counter=17 switch=true /);
        expect(messageOf(r)).toMatch(/keys=.*\bscratch\b/);
      });

      it("typed reads of a value of the wrong type, or of a missing section, fall back to the default", async () => {
        const r = await executeQuery(key, "lua_settings_bad");
        expect(messageOf(r)).toBe("int=-1 bool=false missing=default getSetting=abc");
      });

      it("save writes the settings file", async () => {
        const r = await executeQuery(key, "lua_settings_save");
        expect(messageOf(r)).toBe("saved");
        await until("the saved key in the settings file", () =>
          /^saved\s*=\s*by lua$/m.test(fs.readFileSync(nscp.settingsFile, "utf8")),
        );
      });
    });

    describe("/api/v2/scripts/lua", () => {
      const auth = () => ({ Authorization: `Bearer ${key}` });

      it("is listed as a runtime", async () => {
        const res = await request(REST_URL)
          .get("/api/v2/scripts")
          .set(auth())
          .trustLocalhost(true)
          .expect(200);
        expect(res.body).toEqual(
          expect.arrayContaining([expect.objectContaining({ name: "lua", module: "LUAScript" })]),
        );
      });

      it("lists the script files, and the queries without all=true", async () => {
        const files = await request(REST_URL)
          .get("/api/v2/scripts/lua?all=true")
          .set(auth())
          .trustLocalhost(true)
          .expect(200);
        expect((files.body as string[]).map((f) => f.replace(/\\/g, "/"))).toEqual(
          expect.arrayContaining(["lua/api_fixture.lua", "lua/start_raises.lua"]),
        );

        const queries = await request(REST_URL)
          .get("/api/v2/scripts/lua")
          .set(auth())
          .trustLocalhost(true)
          .expect(200);
        expect(queries.body).toEqual(expect.arrayContaining(["lua_echo"]));
      });

      it("listing the queries, or asking one for its help, never runs a script handler", async () => {
        // Both ask the core for the registered queries. The listing used to ask
        // for every command's parameters too, which the core collects by
        // running the command - so it ran every check on the box, each script
        // handler included, side effects and all.
        await request(REST_URL)
          .get("/api/v2/scripts/lua")
          .set(auth())
          .trustLocalhost(true)
          .expect(200);
        const help = await request(REST_URL)
          .get("/api/v2/queries/lua_calls/help")
          .set(auth())
          .trustLocalhost(true);
        expect(help.status).toBe(200);
        expect(messageOf(await executeQuery(key, "lua_calls"))).toBe("calls=1");
      });

      it("PUT stores a script that loads on the next reload, GET reads it back, DELETE removes it", async () => {
        const source = fs.readFileSync(path.join(FIXTURES, "rest_added.lua"), "utf8");
        await request(REST_URL)
          .put("/api/v2/scripts/lua/rest_added.lua")
          .set(auth())
          .set("Content-Type", "text/plain")
          .send(source)
          .buffer(true)
          .parse(rawText)
          .trustLocalhost(true)
          .expect(200);
        expect(fs.readFileSync(path.join(scripts, "lua", "rest_added.lua"), "utf8")).toBe(source);

        const show = await request(REST_URL)
          .get("/api/v2/scripts/lua/rest_added.lua")
          .set(auth())
          .buffer(true)
          .parse(rawText)
          .trustLocalhost(true)
          .expect(200);
        expect(show.body).toBe(source);

        // Configured, so the next reload loads it.
        await reloadLua();
        expect(messageOf(await executeQuery(key, "lua_rest_added"))).toBe("rest added");

        const del = await request(REST_URL)
          .delete("/api/v2/scripts/lua/rest_added.lua")
          .set(auth())
          .buffer(true)
          .parse(rawText)
          .trustLocalhost(true)
          .expect(200);
        expect(del.body).toMatch(
          /^Deleted .*rest_added\.lua and removed it from \/settings\/lua\/scripts/,
        );
        expect(fs.existsSync(path.join(scripts, "lua", "rest_added.lua"))).toBe(false);

        // Gone from the configuration too, so the next reload does not look
        // for it, and its query goes with it.
        await reloadLua();
        expect(await queryNames(key)).not.toContain("lua_rest_added");
        expect(logLines(nscp, "Failed to find script: rest_added")).toEqual([]);
      });

      it("GET of a script that does not exist is an error, not an empty script", async () => {
        const res = await request(REST_URL)
          .get("/api/v2/scripts/lua/no_such_script.lua")
          .set(auth())
          .buffer(true)
          .parse(rawText)
          .trustLocalhost(true);
        expect(res.status).toBe(500);
        expect(res.body).toMatch(/Script not found: no_such_script\.lua/);
      });

      it("a caller without the add and delete grants can read a script but not change it", async () => {
        const login = await request(REST_URL)
          .get("/api/v2/login")
          .auth("reader", "reader-password")
          .trustLocalhost(true)
          .expect(200);
        const reader = { Authorization: `Bearer ${login.body.key as string}` };
        const file = path.join(scripts, "lua", "api_fixture.lua");
        const before = fs.readFileSync(file, "utf8");

        const show = await request(REST_URL)
          .get("/api/v2/scripts/lua/api_fixture.lua")
          .set(reader)
          .buffer(true)
          .parse(rawText)
          .trustLocalhost(true)
          .expect(200);
        expect(show.body).toBe(before);

        await request(REST_URL)
          .put("/api/v2/scripts/lua/api_fixture.lua")
          .set(reader)
          .set("Content-Type", "text/plain")
          .send("error('replaced')\n")
          .trustLocalhost(true)
          .expect(403);
        await request(REST_URL)
          .delete("/api/v2/scripts/lua/api_fixture.lua")
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
          const link = path.join(scripts, "lua", "rest_leak.lua");
          fs.symlinkSync(secret, link);
          try {
            const show = await request(REST_URL)
              .get("/api/v2/scripts/lua/rest_leak.lua")
              .set(auth())
              .buffer(true)
              .parse(rawText)
              .trustLocalhost(true);
            expect(show.status).toBe(500);
            expect(show.body).toContain("Not allowed outside");
            expect(show.body).not.toContain("not a script");

            await request(REST_URL)
              .delete("/api/v2/scripts/lua/rest_leak.lua")
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

      it.each(["..%2Fnsclient.ini", "lua%2F..%2F..%2Fnsclient.ini", "%2Fetc%2Fpasswd"])(
        "refuses the path %s",
        async (name) => {
          for (const method of ["get", "put", "delete"] as const) {
            const res = await request(REST_URL)
              [method](`/api/v2/scripts/lua/${name}`)
              .set(auth())
              .trustLocalhost(true);
            expect(res.status).toBe(400);
          }
          expect(fs.existsSync(nscp.settingsFile)).toBe(true);
        },
      );
    });

    describe("a reload", () => {
      it("runs the top-level code and on_start again, without duplicating any registration", async () => {
        const [l0, s0] = [loads(), starts()];

        await reloadLua();

        expect(loads()).toBe(l0 + 1);
        expect(starts()).toBe(s0 + 1);
        const names = await queryNames(key);
        expect(names.filter((n) => n === "lua_echo")).toHaveLength(1);
        expect((await executeQuery(key, "lua_echo", { after: "reload" })).result).toBe(OK);
        // The script's state is new: what the old one recorded is gone.
        expect(messageOf(await executeQuery(key, "lua_seen"))).toBe("none");
      });
    });
  });

  describe("from the command line", () => {
    let nscp: NscpInstance;
    let scripts: string;
    let trace: string;

    beforeAll(async () => {
      ({ nscp, scripts, trace } = fixtureInstance("nscp-lua-cli-", ["api_fixture.lua"]));
      await nscp.configure({
        "/modules": { LUAScript: "enabled" },
        "/settings/lua/scripts": { luaapi: "api_fixture.lua" },
        "/settings/luaapi": { trace },
      });
    });

    const lua = (args: string[]) => nscp.run(["lua", ...args], { allowFailure: true });

    it("a simple_cmdline handler answers `nscp client --exec`", async () => {
      const r = await nscp.run(
        ["client", "--module", "LUAScript", "--exec", "lua_cli_echo", "a", "b"],
        { allowFailure: true },
      );
      expect(r.stdout).toContain("cli: a b");
      expect(r.exitCode).toBe(0);
    });

    it("a simple_cmdline handler that raises fails the command and says why", async () => {
      const r = await nscp.run(["client", "--module", "LUAScript", "--exec", "lua_cli_fail"], {
        allowFailure: true,
      });
      expect(r.all).toContain("boom from lua_cli_fail");
      expect(r.exitCode).not.toBe(0);
    });

    it("`--exec` of a command no script registered names it and fails", async () => {
      const r = await nscp.run(["client", "--module", "LUAScript", "--exec", "no_such_cmd"], {
        allowFailure: true,
      });
      expect(r.all).toContain("Command not found: no_such_cmd");
      expect(r.exitCode).not.toBe(0);
    });

    it("`nscp lua` with no verb names the lua verbs", async () => {
      const r = await lua([]);
      expect(r.all).toContain("Usage: nscp lua [add|execute|list|show|install|delete] --help");
      expect(r.all).not.toContain("nscp py");
    });

    it("`nscp lua execute` loads the script, then runs main with the arguments", async () => {
      fs.rmSync(trace, { force: true });
      const r = await lua(["execute", "--script", "api_fixture.lua", "install", "--root", "/tmp"]);
      expect(r.stdout).toContain("main ran with 3 arguments");
      expect(r.exitCode).toBe(0);
      // The configured copy loads and starts with the module; the executed
      // copy is a second instance that loads and runs main, and is never
      // started.
      expect(lifecycle(trace).filter((l) => l.startsWith("main"))).toEqual([
        "main install --root /tmp",
      ]);
    });

    it("`nscp lua execute` names a script it cannot find", async () => {
      const r = await lua(["execute", "--script", "no_such_script.lua"]);
      expect(r.all).toContain("Script not found: no_such_script.lua");
      expect(r.exitCode).not.toBe(0);
    });

    it("`nscp lua execute` of a script with no main says so", async () => {
      fs.copyFileSync(
        path.join(FIXTURES, "rest_added.lua"),
        path.join(scripts, "lua", "no_main.lua"),
      );
      try {
        const r = await lua(["execute", "--script", "no_main.lua"]);
        expect(r.all).toContain("Failed to handle command main");
        expect(r.exitCode).not.toBe(0);
      } finally {
        fs.rmSync(path.join(scripts, "lua", "no_main.lua"));
      }
    });

    it("`nscp lua add` refuses a script that does not exist, and changes nothing", async () => {
      const before = fs.readFileSync(nscp.settingsFile, "utf8");
      const r = await lua(["add", "--script", "no_such_script.lua"]);
      expect(r.all).toContain("Script not found");
      expect(r.exitCode).not.toBe(0);
      expect(fs.readFileSync(nscp.settingsFile, "utf8")).toBe(before);
    });

    it("`nscp lua add --import` will not overwrite a script without --replace", async () => {
      const source = path.join(nscp.workDir, "import_me.lua");
      fs.copyFileSync(path.join(FIXTURES, "rest_added.lua"), source);
      const add = (extra: string[]) =>
        lua(["add", "--script", "import_me.lua", "--import", source, "--no-config", ...extra]);
      expect((await add([])).exitCode).toBe(0);

      fs.writeFileSync(source, "-- a newer copy\n");
      const again = await add([]);
      expect(again.all).toContain("Script already exists, specify --replace to replace it");
      expect(again.exitCode).not.toBe(0);
      const imported = path.join(scripts, "lua", "import_me.lua");
      expect(fs.readFileSync(imported, "utf8")).not.toContain("a newer copy");

      expect((await add(["--replace"])).exitCode).toBe(0);
      expect(fs.readFileSync(imported, "utf8")).toContain("a newer copy");
      fs.rmSync(imported);
    });

    it("`nscp lua list` lists the scripts folder, and --json as an array", async () => {
      const r = await lua(["list"]);
      expect(r.stdout.replace(/\\/g, "/")).toMatch(/^lua\/api_fixture\.lua$/m);
      const j = await lua(["list", "--json"]);
      const files = (JSON.parse(j.stdout.trim().split(/\r?\n/).pop() ?? "[]") as string[]).map(
        (f) => f.replace(/\\/g, "/"),
      );
      expect(files).toEqual(expect.arrayContaining(["lua/api_fixture.lua"]));
    });

    it("`nscp lua add`, `show` and `delete` manage one script", async () => {
      fs.copyFileSync(
        path.join(FIXTURES, "rest_added.lua"),
        path.join(scripts, "lua", "cli_added.lua"),
      );

      const add = await lua(["add", "--script", "cli_added.lua"]);
      expect(add.stdout).toContain("Added cli_added as cli_added.lua");
      expect(fs.readFileSync(nscp.settingsFile, "utf8")).toMatch(
        /^cli_added\s*=\s*cli_added\.lua$/m,
      );

      const show = await lua(["show", "--script", "cli_added.lua"]);
      expect(show.stdout).toContain("lua_rest_added");
      expect(show.exitCode).toBe(0);

      const del = await lua(["delete", "--script", "cli_added"]);
      expect(del.stdout).toContain("Deleted");
      expect(del.stdout).toContain("and removed it from /settings/lua/scripts");
      expect(del.exitCode).toBe(0);
      expect(fs.existsSync(path.join(scripts, "lua", "cli_added.lua"))).toBe(false);
      expect(fs.readFileSync(nscp.settingsFile, "utf8")).not.toMatch(/cli_added/);

      const again = await lua(["show", "--script", "cli_added.lua"]);
      expect(again.all).toContain("Script not found: cli_added.lua");
      expect(again.exitCode).not.toBe(0);
    });

    it("`nscp lua show` and `delete` stay inside the scripts folder", async () => {
      // nsclient.ini sits two levels above ${scripts}/lua.
      for (const verb of ["show", "delete"]) {
        const r = await lua([verb, "--script", "../../nsclient.ini"]);
        expect(r.all).toContain("Not allowed outside");
        expect(r.exitCode).not.toBe(0);
      }
      expect(fs.existsSync(nscp.settingsFile)).toBe(true);
    });

    itOnUnix(
      "`nscp lua show` does not follow a symlink out of the scripts folder, `delete` removes only the link",
      async () => {
        const outside = path.join(nscp.workDir, "outside");
        fs.mkdirSync(outside, { recursive: true });
        const secret = path.join(outside, "secret.lua");
        fs.writeFileSync(secret, "-- not yours\n");
        fs.symlinkSync(secret, path.join(scripts, "lua", "leak.lua"));
        fs.symlinkSync(outside, path.join(scripts, "lua", "linked"));
        try {
          for (const verb of ["show", "delete"]) {
            const r = await lua([verb, "--script", "linked/secret.lua"]);
            expect(r.all).toContain("Not allowed outside");
            expect(r.exitCode).not.toBe(0);
          }
          const show = await lua(["show", "--script", "leak.lua"]);
          expect(show.all).toContain("Not allowed outside");
          expect(show.all).not.toContain("-- not yours");
          expect(show.exitCode).not.toBe(0);
          const del = await lua(["delete", "--script", "leak.lua"]);
          expect(del.exitCode).toBe(0);
          expect(fs.existsSync(path.join(scripts, "lua", "leak.lua"))).toBe(false);
          expect(fs.readFileSync(secret, "utf8")).toBe("-- not yours\n");
        } finally {
          fs.rmSync(path.join(scripts, "lua", "leak.lua"), { force: true });
          fs.rmSync(path.join(scripts, "lua", "linked"), { force: true });
        }
      },
    );

    it("`nscp lua delete` removes every entry that loads the script, however it is written", async () => {
      const file = path.join(scripts, "lua", "many_entries.lua");
      fs.copyFileSync(path.join(FIXTURES, "rest_added.lua"), file);
      const entries = [
        "many_entries.lua =",
        "by_stem = many_entries",
        `by_absolute_path = ${file}`,
        "by_backslash = lua\\many_entries.lua",
        "unrelated = api_fixture.lua",
      ];
      const ini = fs.readFileSync(nscp.settingsFile, "utf8");
      const header = "[/settings/lua/scripts]";
      expect(ini).toContain(header);
      fs.writeFileSync(nscp.settingsFile, ini.replace(header, [header, ...entries].join("\n")));

      const r = await lua(["delete", "--script", "many_entries.lua"]);
      expect(r.stdout).toContain("and removed it from /settings/lua/scripts");
      expect(r.exitCode).toBe(0);
      const after = fs.readFileSync(nscp.settingsFile, "utf8");
      expect(after).not.toMatch(/many_entries|by_stem|by_absolute_path|by_backslash/);
      expect(after).toMatch(/^unrelated\s*=\s*api_fixture\.lua$/m);
      expect(after).toMatch(/^luaapi\s*=\s*api_fixture\.lua$/m);
    });

    // Root reads any file regardless of its mode, so this needs another user.
    itIf(!onWindows && process.getuid?.() !== 0)(
      "`nscp lua show` of a script it cannot read is an error, not an empty script",
      async () => {
        const file = path.join(scripts, "lua", "unreadable.lua");
        fs.writeFileSync(file, "-- secret\n");
        fs.chmodSync(file, 0o000);
        try {
          const r = await lua(["show", "--script", "unreadable.lua"]);
          expect(r.all).toContain("Failed to read");
          expect(r.exitCode).not.toBe(0);
        } finally {
          fs.chmodSync(file, 0o600);
          fs.rmSync(file);
        }
      },
    );

    it("`nscp lua install` adds and removes configured scripts", async () => {
      fs.copyFileSync(
        path.join(FIXTURES, "rest_added.lua"),
        path.join(scripts, "lua", "installed.lua"),
      );
      try {
        const add = await lua(["install", "--add", "installed.lua"]);
        expect(add.exitCode).toBe(0);
        expect(fs.readFileSync(nscp.settingsFile, "utf8")).toMatch(
          /^installed\.lua\s*=\s*installed\.lua$/m,
        );

        const dup = await lua(["install", "--add", "installed.lua"]);
        expect(dup.all).toContain("Failed to add duplicate script: installed.lua");

        const missing = await lua(["install", "--add", "no_such_script.lua"]);
        expect(missing.all).toContain("Failed to find: no_such_script.lua");

        const remove = await lua(["install", "--remove", "installed.lua"]);
        expect(remove.exitCode).toBe(0);
        expect(fs.readFileSync(nscp.settingsFile, "utf8")).not.toMatch(/^installed\.lua\s*=/m);
      } finally {
        fs.rmSync(path.join(scripts, "lua", "installed.lua"), { force: true });
      }
    });
  });

  describe("the complete example in lua.md, loaded unchanged", () => {
    let nscp: NscpInstance;
    let key: string;

    beforeAll(async () => {
      const workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-lua-doc-"));
      const scripts = path.join(workDir, "scripts");
      fs.mkdirSync(path.join(scripts, "lua"), { recursive: true });
      fs.writeFileSync(path.join(scripts, "lua", "example.lua"), completeExample());
      nscp = new NscpInstance({ workDir, pathOverrides: { scripts } });
      // The [/settings/lua/scripts] block the page gives under "Enable it via".
      key = await setupQueryNscp(nscp, "LUAScript", {
        "/modules": { LUAScript: "enabled", WEBServer: "enabled", CheckHelpers: "enabled" },
        "/settings/lua/scripts": { example: "example.lua" },
      });
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("on_start runs once the script is loaded", async () => {
      await until("on_start's log line", () => logLines(nscp, "example.lua ready").length > 0);
    });

    it("check_random answers with a state that matches its value and thresholds", async () => {
      for (let i = 0; i < 5; i++) {
        const r = await executeQuery(key, "check_random");
        const value = Number(perfOf(r).random.value);
        expect(perfOf(r).random).toMatchObject({ warning: 80, critical: 90 });
        expect(value).toBeGreaterThanOrEqual(0);
        expect(value).toBeLessThanOrEqual(100);
        expect(r.result).toBe(value > 90 ? CRITICAL : value > 80 ? WARNING : OK);
        expect(messageOf(r)).toMatch(new RegExp(`^Random value ${value} is (fine|high|too high)$`));
      }
    });

    it("the LOG-SUBMIT handler logs what is submitted on its channel", async () => {
      const res = await checkAndForward(key, {
        command: "check_warning",
        channel: "LOG-SUBMIT",
        arguments: "message=doc example",
      });
      expect(res.result).toBe(OK);
      await until(
        "the handler's log line",
        () => logLines(nscp, "[LOG-SUBMIT] check_warning warning: doc example").length > 0,
      );
    });

    it("say_hello answers `nscp client --exec`", async () => {
      // A second agent on the same settings, so the running one keeps its port.
      const r = await nscp.run(["client", "--module", "LUAScript", "--exec", "say_hello", "docs"], {
        allowFailure: true,
      });
      expect(r.stdout).toContain("Hello, docs");
      expect(r.exitCode).toBe(0);
    });

    it("main answers `nscp lua execute`", async () => {
      const r = await nscp.run(["lua", "execute", "--script", "example.lua", "a", "b"], {
        allowFailure: true,
      });
      expect(r.stdout).toContain("done");
      expect(r.exitCode).toBe(0);
    });
  });
});
