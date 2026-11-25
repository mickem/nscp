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

// The keys documented under `path`: the entries indented under its `keys:`.
function documentedKeys(path: string): string[] {
  // vitest runs with the web/ package root as the working directory.
  const dir = join(process.cwd(), "..", "docs", "reference");
  for (const name of readdirSync(dir).filter((n) => n.endsWith(".yaml"))) {
    const lines = readFileSync(join(dir, name), "utf8").split("\n");
    const at = lines.indexOf(`    ${path}:`);
    if (at < 0) continue;
    const keysAt = lines.findIndex((l, i) => i > at && l === "      keys:");
    const end = lines.findIndex((l, i) => i > at && /^ {0,4}\S/.test(l));
    if (keysAt < 0 || (end >= 0 && keysAt > end)) return [];
    const out: string[] = [];
    for (let i = keysAt + 1; i < lines.length && /^ {8}/.test(lines[i]); ++i) {
      const m = lines[i].match(/^ {8}(\S[^:]*):$/);
      if (m) out.push(m[1]);
    }
    return out;
  }
  return [];
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

  it("names only keys the agent registers as handled by a panel", () => {
    const missing = MODULE_PAGES.flatMap((page) =>
      Object.entries({ ...page.handledKeys, ...page.expertOnlyKeys }).flatMap(([path, keys]) => {
        const documented = documentedKeys(path);
        return keys.filter((k) => !documented.includes(k)).map((k) => `${page.id}: [${path}] ${k}`);
      }),
    );
    expect(missing).toEqual([]);
  });

  it("gives every key of a panel's sections a place", () => {
    // A panel is the whole page outside expert mode, so a key it neither edits
    // nor leaves to expert mode on purpose would only be reachable there.
    const uncovered = MODULE_PAGES.filter((page) => page.Panel).flatMap((page) =>
      Object.keys(page.handledKeys ?? {}).flatMap((path) => {
        const covered = new Set([
          ...(page.handledKeys?.[path] ?? []),
          ...(page.expertOnlyKeys?.[path] ?? []),
        ]);
        return documentedKeys(path)
          .filter((k) => !covered.has(k))
          .map((k) => `${page.id}: [${path}] ${k}`);
      }),
    );
    expect(uncovered).toEqual([]);
  });

  it("looks modules up by id", () => {
    expect(getModulePage("NRPEServer")?.Panel).toBeDefined();
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
