// The bar at the bottom of a module page that is only lists (the clients'
// targets, Check helpers' aliases, ...): the rows the lists have queued, Save
// and Discard, and Show config for the module.
import { useState } from "react";
import { toIni, useApplyChanges } from "./changes.ts";
import ConfigDialog from "./ConfigDialog.tsx";
import { listsIni } from "./listsIni.ts";
import SaveBar from "./SaveBar.tsx";
import { useStagedSettings } from "./staging.ts";

export default function ListsBar({ moduleId, paths }: { moduleId: string; paths: string[] }) {
  const { stored } = useStagedSettings();
  const applyChanges = useApplyChanges();
  const [open, setOpen] = useState(false);
  return (
    <>
      <SaveBar
        moduleId={moduleId}
        changes={[]}
        onSaveChanges={applyChanges}
        onShowConfig={() => setOpen(true)}
      />
      <ConfigDialog
        open={open}
        onClose={() => setOpen(false)}
        title={`${moduleId} configuration`}
        note="Paste it into nsclient.ini on another agent, or into a fleet bundle."
        render={() => {
          const lists = listsIni(stored ?? [], paths);
          const head = toIni([["/modules", [[moduleId, "enabled"]]]]);
          return lists ? `${head}\n\n${lists}` : head;
        }}
      />
    </>
  );
}
