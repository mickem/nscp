// Check helpers' aliases: a fixed check under a name of its own. Unlike an
// External scripts alias it runs no program, so it is the safe way to vary a
// check without letting callers pass arguments.
import AliasEditor from "../kit/AliasEditor.tsx";
import { getKeyValueCollectionConfig } from "../registry.ts";
import type { KeyValueEditorProps } from "../types.ts";

export default function CheckHelpersAliases({ collectionPath }: KeyValueEditorProps) {
  return (
    <AliasEditor
      collectionPath={collectionPath}
      title="Aliases"
      subheader="A check with its arguments fixed, under a name of its own. The monitoring server calls the name; no arguments need to travel."
      presets={getKeyValueCollectionConfig(collectionPath)?.presets}
    />
  );
}
