import { describe, expect, it } from "vitest";
import { readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import {
  findFieldWidget,
  getCollectionConfig,
  getInstanceCollectionConfig,
  getKeyValueCollectionConfig,
  getModulePage,
  MODULE_PAGES,
} from "./registry.ts";

function pathsOf(page: (typeof MODULE_PAGES)[number]): string[] {
  return [...Object.keys(page.collections ?? {}), ...Object.keys(page.sections ?? {})];
}

// Every settings path the reference docs know about. These files are written
// from a real build by scripts/python/docs_extract.py, one per module and
// platform, so a path a module page names that is missing here is a typo or a
// path the C++ side has moved.
function documentedPaths(): Set<string> {
  // vitest runs with the web/ package root as the working directory.
  const dir = join(process.cwd(), "..", "docs", "reference");
  const paths = new Set<string>();
  for (const name of readdirSync(dir)) {
    if (!name.endsWith(".yaml")) continue;
    for (const m of readFileSync(join(dir, name), "utf8").matchAll(
      /^\s+(\/settings\/[^:\n]*):$/gm,
    )) {
      paths.add(m[1]);
    }
  }
  return paths;
}

describe("module page registry", () => {
  it("registers each module once", () => {
    const ids = MODULE_PAGES.map((p) => p.id);
    expect(new Set(ids).size).toBe(ids.length);
  });

  it("gives every path exactly one owner", () => {
    const owners = new Map<string, string[]>();
    for (const page of MODULE_PAGES) {
      for (const path of pathsOf(page)) owners.set(path, [...(owners.get(path) ?? []), page.id]);
    }
    const shared = [...owners].filter(([, ids]) => ids.length > 1);
    expect(shared).toEqual([]);
  });

  it("names only paths the agent registers", () => {
    const documented = documentedPaths();
    const missing = MODULE_PAGES.flatMap((page) =>
      pathsOf(page)
        .filter((path) => !documented.has(path))
        .map((path) => `${page.id}: ${path}`),
    );
    expect(missing).toEqual([]);
  });

  it("looks modules up by id", () => {
    expect(getModulePage("NRPEServer")?.Wizard).toBeDefined();
    expect(getModulePage("NoSuchModule")).toBeUndefined();
  });

  it("resolves collection kinds", () => {
    expect(getCollectionConfig("/settings/system/windows/real-time/checks")?.kind).toBe("hidden");
    expect(getKeyValueCollectionConfig("/settings/WEB/server/roles")).toBeDefined();
    expect(getKeyValueCollectionConfig("/settings/check helpers/alias")?.presets?.length).toBe(13);
    expect(getInstanceCollectionConfig("/settings/WEB/server/roles")).toBeUndefined();
  });

  it("gives Linux the same real-time layout as Windows", () => {
    for (const resource of ["cpu", "memory", "process"]) {
      const windows = getInstanceCollectionConfig(`/settings/system/windows/real-time/${resource}`);
      const unix = getInstanceCollectionConfig(`/settings/system/unix/real-time/${resource}`);
      expect(unix?.fieldGroups).toEqual(windows?.fieldGroups);
      expect(unix?.previewColumn).toEqual(windows?.previewColumn);
      expect(unix?.presets?.length).toBeGreaterThan(0);
    }
  });

  it("finds widgets on sections and on collection instances", () => {
    expect(findFieldWidget("/settings/NRPE/server", "tls version")?.kind).toBe("select");
    expect(findFieldWidget("/settings/system/windows/counters/my_counter", "counter")?.kind).toBe(
      "counter",
    );
    expect(findFieldWidget("/settings/system/windows/counters/default", "flags")?.kind).toBe(
      "multi",
    );
    expect(findFieldWidget("/settings/WEB/server/users/admin", "role")?.kind).toBe("role");
    // The collection root itself is not an instance.
    expect(findFieldWidget("/settings/system/windows/counters", "counter")).toBeUndefined();
    // Nested deeper than one instance level is not an instance either.
    expect(findFieldWidget("/settings/WEB/server/users/admin/x", "role")).toBeUndefined();
  });

  it("keeps the two memory vocabularies apart", () => {
    const windows = findFieldWidget("/settings/system/windows/real-time/memory/m", "type");
    const unix = findFieldWidget("/settings/system/unix/real-time/memory/m", "type");
    expect(windows).toEqual({ kind: "multi", options: ["physical", "committed", "virtual"] });
    expect(unix?.kind).toBe("select");
    expect(unix?.kind === "select" && unix.options.map((o) => o.value)).toEqual([
      "physical",
      "cached",
      "swap",
    ]);
  });
});

describe("module page visibility rules", () => {
  const ctx = { fields: [], isTemplate: false };

  it("shows a counter's buffer size only for the round-robin strategy", () => {
    const rule = getInstanceCollectionConfig("/settings/system/windows/counters")?.isFieldVisible;
    const strategy = (value: string) => [{ key: "collection strategy", value } as never];
    expect(rule?.("buffer size", { ...ctx, fields: strategy("rrd") })).toBe(true);
    expect(rule?.("buffer size", { ...ctx, fields: strategy("static") })).toBe(false);
    expect(rule?.("counter", ctx)).toBeUndefined();
  });

  it("hides a WEB user's alias on the tailored page", () => {
    const rule = getInstanceCollectionConfig("/settings/WEB/server/users")?.isFieldVisible;
    expect(rule?.("alias", ctx)).toBe(false);
    expect(rule?.("role", ctx)).toBeUndefined();
  });
});
