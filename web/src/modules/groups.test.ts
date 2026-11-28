import { describe, expect, it } from "vitest";
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { groupsOf, MODULE_CATEGORIES } from "./groups";

// The in-tree modules: every folder under modules/ with a module.json.
const MODULES_DIR = join(process.cwd(), "..", "modules");
const IN_TREE = readdirSync(MODULES_DIR).filter((d) =>
  existsSync(join(MODULES_DIR, d, "module.json")),
);
const categoryOf = (m: string): unknown =>
  JSON.parse(readFileSync(join(MODULES_DIR, m, "module.json"), "utf8")).module?.category;

describe("module groups", () => {
  it("every in-tree module declares a category the list knows", () => {
    const bad = IN_TREE.filter(
      (m) => !(MODULE_CATEGORIES as readonly unknown[]).includes(categoryOf(m)),
    ).map((m) => `${m}: ${String(categoryOf(m))}`);
    expect(bad).toEqual([]);
  });

  it("files a database or a container under Checks as well", () => {
    expect(groupsOf("database")).toEqual(["checks", "databases"]);
    expect(groupsOf("container")).toEqual(["checks", "containers"]);
    expect(groupsOf("server")).toEqual(["servers"]);
  });

  it("files a tool, or a module without a category, under Other", () => {
    expect(groupsOf("tool")).toEqual(["other"]);
    expect(groupsOf(undefined)).toEqual(["other"]);
    expect(groupsOf("something-new")).toEqual(["other"]);
  });
});
