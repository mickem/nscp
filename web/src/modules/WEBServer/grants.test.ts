import { describe, expect, it } from "vitest";
import { readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import {
  formatGrants,
  GRANT_AREAS,
  grants,
  holds,
  KNOWN_GRANTS,
  otherGrants,
  parseGrants,
  powerfulHeld,
} from "./grants";

describe("grant matching, as the agent does it", () => {
  it.each([
    ["*", "settings.put", true],
    ["modules.*", "modules.load", true],
    ["modules.*", "modules", false],
    ["queries.execute", "queries.execute", true],
    ["queries", "queries.execute", false],
    ["queries.execute", "queries", false],
    ["queries.execute", "queries.execute.noargs", false],
    ["scripts.*", "scripts.add.lua", true],
  ])("%s grants %s: %s", (rule, need, expected) => {
    expect(grants(rule, need)).toBe(expected);
  });

  it("reads and writes a role's grant string", () => {
    expect(parseGrants(" public, login.get ,,")).toEqual(["public", "login.get"]);
    expect(formatGrants(["public", "login.get"])).toBe("public,login.get");
  });

  it("names the powerful grants a role holds, wildcards included", () => {
    expect(powerfulHeld(["public", "metrics.list"])).toEqual([]);
    expect(powerfulHeld(["settings.*"])).toEqual(["Change settings", "Delete settings"]);
    expect(holds(["*"], "console.exec")).toBe(true);
  });

  it("keeps grants it does not offer", () => {
    expect(otherGrants(["public", "modules.*", "*", "custom.thing"])).toEqual(["custom.thing"]);
  });
});

describe("the grant catalogue", () => {
  // Every grant the web server's controllers ask for.
  const asked = (() => {
    // vitest runs with the web/ package root as the working directory.
    const dir = join(process.cwd(), "..", "modules", "WEBServer");
    const out = new Set<string>();
    for (const name of readdirSync(dir).filter((n) => n.endsWith(".cpp") && !n.includes("_test"))) {
      const text = readFileSync(join(dir, name), "utf8");
      for (const m of text.matchAll(/(?:is_logged_in|can|has_grant)\("([a-z_.]+)"/g)) out.add(m[1]);
      // Alternatives: grant_options{"a", "b"}.
      for (const m of text.matchAll(/grant_options\{([^}]*)\}/g))
        for (const g of m[1].matchAll(/"([a-z_.]+)"/g)) out.add(g[1]);
    }
    // `anonymous` is the role unauthenticated callers get, not a grant.
    out.delete("anonymous");
    return out;
  })();

  // scripts.<verb>.<runtime> is asked for as "scripts.<verb>." + runtime.
  const isAsked = (id: string) => asked.has(id) || asked.has(id.slice(0, id.lastIndexOf(".") + 1));

  it("offers only grants the agent asks for", () => {
    expect(KNOWN_GRANTS.map((g) => g.id).filter((id) => !isAsked(id))).toEqual([]);
  });

  it("offers every grant the agent asks for", () => {
    const offered = KNOWN_GRANTS.map((g) => g.id);
    const missing = [...asked].filter(
      (a) => !offered.includes(a) && !offered.some((o) => a.endsWith(".") && o.startsWith(a)),
    );
    expect(missing).toEqual([]);
  });

  it("gives every wildcard something in its own area to cover", () => {
    for (const area of GRANT_AREAS.filter((a) => a.wildcard))
      expect(area.grants.some((g) => grants(area.wildcard!, g.id))).toBe(true);
  });
});
