import { describe, expect, it } from "vitest";
import { describeChange, isEnabledValue, pendingChanges, readStored } from "./changes";
import { allowedHostsChange, isOpenToAll, readAllowedHosts } from "./allowedHosts";

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

describe("allowed hosts", () => {
  it("resolves the value in effect and where it comes from", () => {
    expect(readAllowedHosts([], "/srv")).toEqual({ value: "127.0.0.1", source: "built-in" });
    expect(readAllowedHosts([s("/settings/default", "allowed hosts", "10.0.0.1")], "/srv")).toEqual(
      { value: "10.0.0.1", source: "default section" },
    );
    expect(
      readAllowedHosts(
        [
          s("/settings/default", "allowed hosts", "10.0.0.1"),
          s("/srv", "allowed hosts", "10.0.0.2"),
        ],
        "/srv",
      ),
    ).toEqual({ value: "10.0.0.2", source: "own" });
  });

  it("writes to the server's own path, and only when the value in effect changes", () => {
    const stored = [s("/settings/default", "allowed hosts", "10.0.0.1")];
    expect(pendingChanges(stored, [allowedHostsChange(stored, "/srv", "10.0.0.1")])).toEqual([]);
    expect(pendingChanges(stored, [allowedHostsChange(stored, "/srv", "10.0.0.9")])).toEqual([
      { path: "/srv", key: "allowed hosts", value: "10.0.0.9", defaultValue: "10.0.0.1" },
    ]);
  });

  it("recognises a list open to everyone", () => {
    expect(isOpenToAll("10.0.0.1, *")).toBe(true);
    expect(isOpenToAll("0.0.0.0/0")).toBe(true);
    expect(isOpenToAll("10.0.0.0/8,127.0.0.1")).toBe(false);
  });
});
