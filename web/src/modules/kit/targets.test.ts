import { describe, expect, it } from "vitest";
import { Settings } from "../../api/api.ts";
import {
  advancedKeys,
  effective,
  endpoint,
  targetChanges,
  targetIni,
  targetNames,
  TargetSpec,
  targetValues,
} from "./targets.ts";

const P = "/settings/X/client/targets";
const spec: TargetSpec = {
  moduleId: "XClient",
  path: P,
  noun: "X server",
  fields: {
    address: { label: "Address", kind: "text", builtIn: "" },
    port: { label: "Port", kind: "number", builtIn: "1234" },
    "use ssl": { label: "TLS", kind: "bool", builtIn: "true" },
    secret: { label: "Secret", kind: "secret", builtIn: "" },
  },
  sections: [{ title: "Connection", keys: ["address"] }],
  expertOnly: ["secret"],
  risks: () => [],
  security: () => "",
};

const stored: Settings[] = [
  { path: `${P}/default`, key: "use ssl", value: "false" },
  { path: `${P}/a`, key: "address", value: "a.example" },
  { path: `${P}/b`, key: "parent", value: "a" },
  { path: `${P}/c`, key: "parent", value: "c" },
  { path: P, key: "d", value: "d" },
];

describe("targets", () => {
  it("lists default first, then every stored or named target", () => {
    expect(targetNames(stored, spec)).toEqual(["default", "a", "b", "c", "d"]);
    expect(targetNames([], spec)).toEqual(["default"]);
  });

  it("inherits from the parent, then default, then the built-in", () => {
    expect(effective(stored, spec, "a", "use ssl")).toBe("false");
    // Every key is inherited, a key the agent registers with a default and
    // the address included.
    expect(effective(stored, spec, "b", "address")).toBe("a.example");
    expect(effective(stored, spec, "b", "port")).toBe("1234");
    expect(effective(stored, spec, "b", "use ssl")).toBe("false");
    expect(effective(stored, spec, "default", "port")).toBe("1234");
    // A loop falls back to the built-in rather than spin.
    expect(effective(stored, spec, "c", "use ssl")).toBe("true");
  });

  it("takes a key with a built-in default from the parent, not the built-in", () => {
    const withPort: Settings[] = [
      { path: `${P}/default`, key: "port", value: "99" },
      { path: `${P}/default`, key: "address", value: "default.example" },
      { path: `${P}/tpl`, key: "port", value: "77" },
      { path: `${P}/leaf`, key: "parent", value: "tpl" },
      { path: `${P}/own`, key: "address", value: "own.example" },
      { path: `${P}/root`, key: "parent", value: "" },
      { path: P, key: "short", value: "short.example:5" },
    ];
    expect(effective(withPort, spec, "plain", "port")).toBe("99");
    expect(effective(withPort, spec, "leaf", "port")).toBe("77");
    expect(effective(withPort, spec, "leaf", "address")).toBe("default.example");
    // A target at an address of its own does not take its parent's port.
    expect(effective(withPort, spec, "own", "port")).toBe("1234");
    // A one-line target is at the address it names.
    expect(effective(withPort, spec, "short", "address")).toBe("short.example:5");
    expect(effective(withPort, spec, "short", "port")).toBe("1234");
    // An empty parent makes a root, like default.
    expect(effective(withPort, spec, "root", "port")).toBe("1234");
  });

  it("writes only what differs from what the target inherits", () => {
    const changes = targetChanges(stored, spec, "a", { "use ssl": "false", port: "99" });
    expect(changes).toEqual([
      { path: `${P}/a`, key: "use ssl", value: "false", defaultValue: "false" },
      { path: `${P}/a`, key: "port", value: "99", defaultValue: "1234" },
    ]);
  });

  it("reads a draft over the stored values", () => {
    const v = targetValues(stored, spec, "a", { port: "5" });
    expect(endpoint(v)).toBe("a.example");
    expect(v("port")).toBe("5");
    expect(endpoint((k) => ({ host: "h", port: "7" })[k] ?? "")).toBe("h:7");
  });

  it("puts unnamed keys under Advanced, leaving expert-only ones out", () => {
    expect(advancedKeys(spec)).toEqual(["port", "use ssl"]);
  });

  it("prints a target with the default it leans on, or pinned", () => {
    expect(targetIni(stored, spec, "a", false)).toBe(
      `[/modules]\nXClient = enabled\n\n[${P}/default]\nuse ssl = false\n\n[${P}/a]\naddress = a.example`,
    );
    expect(targetIni(stored, spec, "a", true)).toContain("port = 1234\nuse ssl = false");
  });
});
