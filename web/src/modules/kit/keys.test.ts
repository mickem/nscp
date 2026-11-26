import { describe, expect, it } from "vitest";
import { Settings } from "../../api/api";
import { pendingChanges } from "./changes";
import { fixed, keyChanges, KeyTable, readKey, tableIni, tableKeys } from "./keys";

const P = "/settings/M";
const s = (path: string, key: string, value: string): Settings => ({ path, key, value });

const TABLE: KeyTable = {
  [P]: {
    port: { builtIn: fixed("1") },
    host: { builtIn: fixed("h"), inherits: true },
    name: { builtIn: fixed(""), computedDefault: true },
  },
  [`${P}/log`]: { debug: { builtIn: fixed("false") } },
};

describe("key tables", () => {
  it("reads a key with or without a scope", () => {
    expect(readKey([], P, "port", TABLE[P].port)).toEqual({ value: "1" });
    expect(readKey([s("/settings/default", "host", "x")], P, "host", TABLE[P].host)).toEqual({
      scope: "default",
      value: "x",
    });
  });

  it("writes a plain key only when it changes", () => {
    expect(pendingChanges([], keyChanges([], P, "port", TABLE[P].port, { value: "1" }))).toEqual(
      [],
    );
    expect(pendingChanges([], keyChanges([], P, "port", TABLE[P].port, { value: "2" }))).toEqual([
      { path: P, key: "port", value: "2", defaultValue: "1" },
    ]);
  });

  it("lists every key per path", () => {
    expect(tableKeys(TABLE)).toEqual({ [P]: ["port", "host", "name"], [`${P}/log`]: ["debug"] });
  });

  it("renders the INI fragment, each key in the section the agent reads it from", () => {
    const stored = [s(P, "port", "9"), s("/settings/default", "host", "x"), s(P, "name", "me")];
    expect(tableIni(stored, "M", TABLE, false)).toBe(
      [
        "[/modules]",
        "M = enabled",
        "",
        "[/settings/default]",
        "host = x",
        "",
        `[${P}]`,
        "port = 9",
        "name = me",
      ].join("\n"),
    );
  });

  it("adds defaults on request, but never one the agent works out itself", () => {
    const ini = tableIni([], "M", TABLE, true);
    expect(ini).toContain("[/settings/default]\nhost = h");
    expect(ini).toContain(`[${P}]\nport = 1`);
    expect(ini).toContain(`[${P}/log]\ndebug = false`);
    expect(ini).not.toContain("name =");
  });
});
