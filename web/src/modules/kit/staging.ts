// The writes a module page's lists have queued: a list dialog stages its
// rows here and the page's Save writes them with the panel's own changes,
// so nothing reaches the agent before the reader says so. The lists, the
// panel and Show config read the settings with the queue applied.
//
// Outside a module page (no provider) a list writes straight away, as it
// always did.
import { createContext, useCallback, useContext } from "react";
import {
  Settings,
  useDeleteSettingsMutation,
  useGetSettingsQuery,
  useUpdateSettingsMutation,
} from "../../api/api.ts";

export type StagedOp =
  | { kind: "set"; path: string; key: string; value: string }
  // One key.
  | { kind: "delete"; path: string; key: string }
  // A section and every section below it (a target, a schedule, ...).
  | { kind: "deletePath"; path: string };

export interface Staging {
  ops: StagedOp[];
  stage: (ops: StagedOp[]) => void;
  clear: () => void;
}

export const StagingContext = createContext<Staging | null>(null);

const under = (p: string, root: string) => p === root || p.startsWith(root + "/");

// The stored settings as they will be once the queue is written.
export function applyOps(stored: Settings[] | undefined, ops: StagedOp[]): Settings[] {
  let out = [...(stored ?? [])];
  for (const op of ops) {
    if (op.kind === "deletePath") {
      out = out.filter((s) => !under(s.path, op.path));
      continue;
    }
    out = out.filter((s) => !(s.path === op.path && s.key === op.key));
    if (op.kind === "set") out.push({ path: op.path, key: op.key, value: op.value });
  }
  return out;
}

// One line per queued write, for the save bar.
export function describeOp(op: StagedOp): string {
  if (op.kind === "set") return `[${op.path}] ${op.key} = ${op.value}`;
  if (op.kind === "delete") return `[${op.path}] ${op.key} removed`;
  return `[${op.path}] removed, with everything under it`;
}

export function useWriteOps(): (ops: StagedOp[]) => Promise<void> {
  const [updateSettings] = useUpdateSettingsMutation();
  const [deleteSettings] = useDeleteSettingsMutation();
  return useCallback(
    async (ops: StagedOp[]) => {
      for (const op of ops) {
        if (op.kind === "set")
          await updateSettings({ path: op.path, key: op.key, value: op.value }).unwrap();
        else if (op.kind === "delete")
          await deleteSettings({ path: op.path, key: op.key }).unwrap();
        else await deleteSettings({ path: op.path }).unwrap();
      }
    },
    [updateSettings, deleteSettings],
  );
}

// What a list reads and how it writes: on a module page, the settings with
// the queue applied and a write that queues; elsewhere the stored settings
// and a write that goes to the agent.
export function useStagedSettings(): {
  stored: Settings[] | undefined;
  write: (ops: StagedOp[]) => Promise<void>;
  staged: boolean;
} {
  const { data } = useGetSettingsQuery();
  const staging = useContext(StagingContext);
  const writeNow = useWriteOps();
  if (!staging) return { stored: data, write: writeNow, staged: false };
  return {
    stored: data === undefined ? undefined : applyOps(data, staging.ops),
    write: async (ops) => staging.stage(ops),
    staged: true,
  };
}

// A page's changes (a dialog's draft, ...) as queued writes; a null value
// removes the key.
export function changesToOps(
  changes: Array<{ path: string; key: string; value: string | null }>,
): StagedOp[] {
  return changes.map((c) =>
    c.value === null
      ? { kind: "delete", path: c.path, key: c.key }
      : { kind: "set", path: c.path, key: c.key, value: c.value },
  );
}
