import { describe, expect, it } from "vitest";
import {
  applyToStored,
  describeChange,
  isEnabledValue,
  pendingChanges,
  readStored,
  scopedChanges,
  toIni,
  valueInScope,
} from "./changes";
import { allowedHostsChanges, isOpenToAll, readAllowedHosts } from "./allowedHosts";

const s = (path: string, key: string, value: string) => ({ path, key, value });

describe("setting changes", () => {
  it("reads the last stored value of a key", () => {
    expect(readStored([s("/a", "k", "1"), s("/a", "k", "2")], "/a", "k")).toBe("2");
    expect(readStored(undefined, "/a", "k")).toBeUndefined();
  });

  it("drops changes that change nothing", () => {
    const stored = [s("/a", "set", "x")];
    expect(
      pendingChanges(stored, [
        { path: "/a", key: "set", value: "x" },
        { path: "/a", key: "unset", value: "d", defaultValue: "d" },
        { path: "/a", key: "unset", value: null },
        { path: "/a", key: "unset", value: "" },
      ]),
    ).toEqual([]);
  });

  it("keeps real changes, the last one per key", () => {
    const stored = [s("/a", "set", "x")];
    expect(
      pendingChanges(stored, [
        { path: "/a", key: "set", value: "y" },
        { path: "/a", key: "set", value: null },
        { path: "/a", key: "new", value: "v", defaultValue: "d" },
      ]),
    ).toEqual([
      { path: "/a", key: "set", value: null },
      { path: "/a", key: "new", value: "v", defaultValue: "d" },
    ]);
  });

  it("describes a change", () => {
    expect(describeChange({ path: "/a", key: "k", value: "v" })).toBe("[/a] k = v");
    expect(describeChange({ path: "/a", key: "k", value: null })).toBe("[/a] k: reset to default");
  });

  it("reads a module as enabled in every spelling the core writes", () => {
    for (const v of ["enabled", "true", "1", "Enabled"]) expect(isEnabledValue(v)).toBe(true);
    for (const v of ["disabled", "false", "", undefined]) expect(isEnabledValue(v)).toBe(false);
  });
});

describe("allowed hosts and other inheritable keys", () => {
  it("reads the scope and value in effect", () => {
    expect(readAllowedHosts([], "/srv")).toEqual({ scope: "default", value: "127.0.0.1" });
    expect(readAllowedHosts([s("/settings/default", "allowed hosts", "10.0.0.1")], "/srv")).toEqual(
      { scope: "default", value: "10.0.0.1" },
    );
    expect(
      readAllowedHosts(
        [
          s("/settings/default", "allowed hosts", "10.0.0.1"),
          s("/srv", "allowed hosts", "10.0.0.2"),
        ],
        "/srv",
      ),
    ).toEqual({ scope: "local", value: "10.0.0.2" });
  });

  it("writes the shared section in the default scope, only when the value changes", () => {
    const stored = [s("/settings/default", "allowed hosts", "10.0.0.1")];
    const keep = allowedHostsChanges("/srv", { scope: "default", value: "10.0.0.1" });
    expect(pendingChanges(stored, keep)).toEqual([]);
    const change = allowedHostsChanges("/srv", { scope: "default", value: "10.0.0.9" });
    expect(pendingChanges(stored, change)).toEqual([
      {
        path: "/settings/default",
        key: "allowed hosts",
        value: "10.0.0.9",
        defaultValue: "127.0.0.1",
      },
    ]);
  });

  it("pins the value locally when switched to local, even unchanged", () => {
    const stored = [s("/settings/default", "allowed hosts", "10.0.0.1")];
    expect(
      pendingChanges(stored, allowedHostsChanges("/srv", { scope: "local", value: "10.0.0.1" })),
    ).toEqual([{ path: "/srv", key: "allowed hosts", value: "10.0.0.1", pin: true }]);
  });

  it("drops the local value when switched back to default", () => {
    const stored = [
      s("/settings/default", "allowed hosts", "10.0.0.1"),
      s("/srv", "allowed hosts", "10.0.0.2"),
    ];
    const next = valueInScope(stored, "/srv", "allowed hosts", "127.0.0.1", "default", "10.0.0.2");
    expect(next).toBe("10.0.0.1");
    expect(
      pendingChanges(
        stored,
        scopedChanges("/srv", "allowed hosts", { scope: "default", value: next }, "127.0.0.1"),
      ),
    ).toEqual([{ path: "/srv", key: "allowed hosts", value: null }]);
  });

  it("starts a local value from what was shown", () => {
    const stored = [s("/settings/default", "timeout", "60")];
    expect(valueInScope(stored, "/srv", "timeout", "30", "local", "60")).toBe("60");
  });

  it("recognises a list open to everyone", () => {
    expect(isOpenToAll("10.0.0.1, *")).toBe(true);
    expect(isOpenToAll("0.0.0.0/0")).toBe(true);
    expect(isOpenToAll("10.0.0.0/8,127.0.0.1")).toBe(false);
  });
});

describe("previews", () => {
  it("applies changes to the stored list", () => {
    expect(
      applyToStored(
        [s("/a", "k", "1"), s("/a", "gone", "x")],
        [
          { path: "/a", key: "k", value: "2" },
          { path: "/a", key: "gone", value: null },
          { path: "/b", key: "new", value: "v" },
        ],
      ),
    ).toEqual([s("/a", "k", "2"), s("/b", "new", "v")]);
  });

  it("renders an INI fragment, leaving out empty sections", () => {
    expect(
      toIni([
        ["/modules", [["NRPEServer", "enabled"]]],
        ["/settings/empty", []],
        [
          "/settings/x",
          [
            ["a", "1"],
            ["b", ""],
          ],
        ],
      ]),
    ).toBe("[/modules]\nNRPEServer = enabled\n\n[/settings/x]\na = 1\nb = ");
  });
});
