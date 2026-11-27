// A client module's targets collection, edited with the targets card.
import type { InstanceCollectionConfig, InstanceEditorProps } from "../types.ts";
import type { TargetSpec } from "./targets.ts";
import TargetsEditor from "./TargetsEditor.tsx";

export function targetsCollection(spec: TargetSpec): InstanceCollectionConfig {
  const Editor = ({ onExpertMode }: InstanceEditorProps) => (
    <TargetsEditor spec={spec} onExpertMode={onExpertMode} />
  );
  Editor.displayName = `${spec.moduleId}Targets`;
  return { kind: "instances", Editor };
}
