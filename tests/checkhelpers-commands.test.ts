/**
 * Every command CheckHelpers exposes, plus the alias section that lets an
 * operator run aliases without enabling CheckExternalScripts.
 *
 * Port of scripts/python/test_check_helpers.py (the legacy `nscp unit` suite).
 * CheckExternalScripts is deliberately not loaded: the aliases below live under
 * `[/settings/check helpers/alias]`, the location admins who want
 * aliases-without-scripts use (docs/docs/setup/securing.md), and proving that
 * works on its own is the point of the alias cases.
 *
 * Run over the one-shot client-query path (`nscp client --module ... --boot
 * --query`), which needs no web server and still passes `k=v` as single tokens,
 * exactly as REST does. That path prints the raw Nagios message with no status
 * word, so the verdict is read from the exit code.
 */
import { NscpInstance, OK, WARNING, CRITICAL } from "@fixtures/index";

jest.setTimeout(180_000);

describe("CheckHelpers commands", () => {
  let nscp: NscpInstance;

  /** Run a CheckHelpers query and return its output and Nagios exit code. */
  async function query(
    command: string,
    args: string[] = [],
  ): Promise<{ out: string; code: number }> {
    const r = await nscp.run(
      ["client", "--module", "CheckHelpers", "--boot", "--query", command, ...args],
      {
        allowFailure: true,
      },
    );
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": { CheckHelpers: "enabled" },
      // Prefixed `tch_` so they cannot collide with anything else.
      "/settings/check helpers/alias": {
        tch_alias_ok: 'check_ok message="fixed via alias"',
        tch_alias_warn: 'check_warning message="warn via alias"',
        tch_alias_msg: 'check_ok "message=$ARG1$"',
        tch_alias_msg_pct: 'check_ok "message=%ARG1%"',
      },
    });
  });

  describe("status commands", () => {
    it.each([
      ["check_ok", OK],
      ["check_warning", WARNING],
      ["check_critical", CRITICAL],
    ])("%s returns its status by default", async (command, status) => {
      const { code } = await query(command);
      expect(code).toBe(status);
    });

    it.each([
      ["check_ok", OK, "hello"],
      ["check_warning", WARNING, "careful"],
      ["check_critical", CRITICAL, "on fire"],
    ])("%s echoes message=", async (command, status, message) => {
      const { out, code } = await query(command, [`message=${message}`]);
      expect(code).toBe(status);
      expect(out).toContain(message);
    });
  });

  it("check_version returns a non-empty version", async () => {
    const { out, code } = await query("check_version");
    expect(code).toBe(OK);
    expect(out.trim()).not.toBe("");
  });

  describe("check_always_* coerce the wrapped result", () => {
    // Positional arguments: the first token is the wrapped command, the rest
    // are forwarded to it. `command=` / `arguments=` belong to check_negate and
    // check_multi.
    it.each([
      ["check_always_ok", "check_critical", OK],
      ["check_always_warning", "check_ok", WARNING],
      ["check_always_critical", "check_ok", CRITICAL],
    ])("%s wrapping %s", async (wrapper, inner, status) => {
      const { code } = await query(wrapper, [inner, "message=ignored"]);
      expect(code).toBe(status);
    });
  });

  describe("check_multi returns the worst status", () => {
    it.each([
      [["check_ok"], OK],
      [["check_ok", "check_warning"], WARNING],
      [["check_ok", "check_warning", "check_critical"], CRITICAL],
    ])("%j", async (commands, status) => {
      const { code } = await query(
        "check_multi",
        commands.map((c) => `command=${c}`),
      );
      expect(code).toBe(status);
    });
  });

  describe("check_negate remaps the status", () => {
    it("ok=critical turns check_ok CRITICAL", async () => {
      const { code } = await query("check_negate", ["ok=critical", "command=check_ok"]);
      expect(code).toBe(CRITICAL);
    });

    it("critical=ok turns check_critical OK", async () => {
      const { code } = await query("check_negate", ["critical=ok", "command=check_critical"]);
      expect(code).toBe(OK);
    });
  });

  it("check_timeout passes a fast command through", async () => {
    // A generous timeout against a fast internal command, so the verdict does
    // not depend on how busy the CI agent is.
    const { out, code } = await query("check_timeout", [
      "timeout=10",
      "command=check_ok",
      "arguments=message=fast",
    ]);
    expect(code).toBe(OK);
    expect(out).toContain("fast");
  });

  describe("aliases under [/settings/check helpers/alias]", () => {
    it("dispatches to check_ok with a fixed message", async () => {
      const { out, code } = await query("tch_alias_ok");
      expect(code).toBe(OK);
      expect(out).toContain("fixed via alias");
    });

    it("dispatches to a non-OK command too", async () => {
      const { out, code } = await query("tch_alias_warn");
      expect(code).toBe(WARNING);
      expect(out).toContain("warn via alias");
    });

    it("substitutes $ARG1$ into the declared arguments", async () => {
      const { out, code } = await query("tch_alias_msg", ["hello via $arg"]);
      expect(code).toBe(OK);
      expect(out).toContain("hello via $arg");
    });

    it("substitutes %ARG1% the same way", async () => {
      const { out, code } = await query("tch_alias_msg_pct", ["hello via pct"]);
      expect(code).toBe(OK);
      expect(out).toContain("hello via pct");
    });

    it("leaves a missing $ARG1$ in place rather than refusing the call", async () => {
      // Pins current behaviour: too few arguments are not an error, the
      // placeholder reaches check_ok verbatim.
      const { out, code } = await query("tch_alias_msg");
      expect(code).toBe(OK);
      expect(out).toContain("$ARG1$");
    });
  });
});
