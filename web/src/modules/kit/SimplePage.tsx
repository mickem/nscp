// A module page that is a handful of keys of one section: the fields, the
// rarely changed ones under Advanced, and the shell's Save and Show config.
import type { ComponentType } from "react";
import type { ModulePanelProps } from "../types.ts";
import { readStored } from "./changes.ts";
import { KeyTable, tableIni } from "./keys.ts";
import PanelShell, { AdvancedSection, KeyField, PanelSection } from "./PanelShell.tsx";
import type { FieldSpec } from "./socketServer.ts";
import { usePanel } from "./usePanel.ts";

export interface SimplePageSpec {
  moduleId: string;
  title: string;
  subheader: string;
  path: string;
  table: KeyTable;
  section: string;
  fields: FieldSpec[];
  advanced?: FieldSpec[];
}

export function simplePage(spec: SimplePageSpec): ComponentType<ModulePanelProps> {
  const Page = () => {
    const panel = usePanel(spec.moduleId, spec.table);
    const changes = panel.pending();
    const f = (s: FieldSpec) => (
      <KeyField
        key={s.key}
        panel={panel}
        path={spec.path}
        k={s.key}
        label={s.label}
        kind={s.kind}
        help={s.help}
      />
    );
    const advanced = spec.advanced ?? [];
    const advancedSet = advanced.filter(
      (a) => readStored(panel.stored, spec.path, a.key) !== undefined,
    ).length;
    return (
      <PanelShell
        title={spec.title}
        subheader={spec.subheader}
        panel={panel}
        changes={changes}
        problem={null}
        ini={(stored, includeDefaults) =>
          tableIni(stored, spec.moduleId, spec.table, includeDefaults)
        }
      >
        <PanelSection title={spec.section}>{spec.fields.map(f)}</PanelSection>
        {advanced.length > 0 && (
          <AdvancedSection count={advancedSet}>{advanced.map(f)}</AdvancedSection>
        )}
      </PanelShell>
    );
  };
  Page.displayName = `${spec.moduleId}Page`;
  return Page;
}
