// Collections whose entries come in two forms (object_handler, include/
// nscapi/settings/object.hpp): a one-line `name = value` under the collection
// path, where the value is the entry's main key (a script's or a schedule's
// command), or a section `<path>/<name>` holding keys. `default` is the
// template every entry takes its unset keys from, never an entry itself.
import { Settings } from "../../api/api.ts";

export interface Entry {
  name: string;
  form: "line" | "section";
  values: Record<string, string>;
}

export function readEntries(
  stored: Settings[] | undefined,
  path: string,
  lineKey: string,
): Entry[] {
  const byName = new Map<string, Entry>();
  const prefix = path + "/";
  for (const s of stored ?? []) {
    if (s.path === path && s.key !== "") {
      if (byName.get(s.key)?.form === "section") continue;
      byName.set(s.key, { name: s.key, form: "line", values: { [lineKey]: s.value } });
    } else if (s.path.startsWith(prefix) && s.key !== "") {
      const name = s.path.slice(prefix.length);
      if (name.includes("/") || name === "default") continue;
      const cur = byName.get(name);
      const values = cur?.form === "section" ? cur.values : {};
      values[s.key] = s.value;
      byName.set(name, { name, form: "section", values });
    }
  }
  return [...byName.values()].sort((a, b) => a.name.localeCompare(b.name));
}

export interface Write {
  kind: "put" | "delete" | "delete-path";
  path: string;
  key?: string;
  value?: string;
}

// What saving an entry writes. Only its main key set: kept as (or turned
// into) one line. Anything else set: a section. A section stays a section.
export function entryWrites(
  path: string,
  keys: readonly string[],
  lineKey: string,
  original: Entry | null,
  name: string,
  values: Record<string, string>,
): Write[] {
  const out: Write[] = [];
  const extra = Object.entries(values).some(([k, v]) => k !== lineKey && v !== "");
  const section = extra || (original?.form === "section" && original.name === name);
  const renamed = original !== null && original.name !== name;
  if (original && (renamed || original.form !== (section ? "section" : "line"))) {
    out.push(
      original.form === "line"
        ? { kind: "delete", path, key: original.name }
        : { kind: "delete-path", path: `${path}/${original.name}` },
    );
  }
  if (!section) {
    out.push({ kind: "put", path, key: name, value: values[lineKey] ?? "" });
    return out;
  }
  const own = `${path}/${name}`;
  for (const key of keys) {
    const v = values[key] ?? "";
    const before =
      original?.name === name && original.form === "section" ? original.values[key] : undefined;
    if (v !== "" && v !== before) out.push({ kind: "put", path: own, key, value: v });
    else if (v === "" && before !== undefined) out.push({ kind: "delete", path: own, key });
  }
  return out;
}

export function deleteEntry(path: string, entry: Entry): Write {
  return entry.form === "line"
    ? { kind: "delete", path, key: entry.name }
    : { kind: "delete-path", path: `${path}/${entry.name}` };
}
