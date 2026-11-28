import { describe, expect, it } from "vitest";
import { readdirSync, existsSync } from "node:fs";
import { join } from "node:path";
import { groupsOf, MODULE_GROUPS } from "./groups";

// The in-tree modules: every folder under modules/ with a module.json.
const MODULES_DIR = join(process.cwd(), "..", "modules");
const IN_TREE = readdirSync(MODULES_DIR).filter((d) =>
  existsSync(join(MODULES_DIR, d, "module.json")),
);
// Left to Other on purpose: tools and samples, not something to find by kind.
const OTHER = ["CommandClient", "CauseCrashes", "SamplePluginSimple"];

describe("module groups", () => {
  it("put every in-tree module in a group, so a new one is not lost under Other", () => {
    const missing = IN_TREE.filter((m) => !OTHER.includes(m) && groupsOf(m)[0] === "other");
    expect(missing).toEqual([]);
  });

  it("name only modules that exist", () => {
    const unknown = MODULE_GROUPS.flatMap((g) => g.modules).filter((m) => !IN_TREE.includes(m));
    expect(unknown).toEqual([]);
  });

  it("put a database in Checks as well", () => {
    expect(groupsOf("CheckMSSQL")).toEqual(["checks", "databases"]);
    expect(groupsOf("my_bundle")).toEqual(["other"]);
  });
});
