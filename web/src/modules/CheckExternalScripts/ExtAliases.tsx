// External scripts' aliases. They work, but an alias belongs in Check
// helpers: the same thing, without External scripts loaded.
import { Stack } from "@mui/material";
import { useGetSettingsQuery } from "../../api/api.ts";
import AliasEditor from "../kit/AliasEditor.tsx";
import Explainer from "../kit/Explainer.tsx";
import type { InstanceEditorProps } from "../types.ts";

export default function ExtAliases({ collectionPath, onExpertMode }: InstanceEditorProps) {
  const { data: stored } = useGetSettingsQuery();
  // Aliases written as sections ([.../alias/<name>] command = ...) are left to
  // expert mode; the list edits the one-line form.
  const sections = new Set(
    (stored ?? [])
      .filter((s) => s.path.startsWith(collectionPath + "/") && s.key !== "")
      .map((s) => s.path.slice(collectionPath.length + 1).split("/")[0])
      .filter((n) => n !== "default"),
  );
  return (
    <Stack spacing={1}>
      <AliasEditor
        collectionPath={collectionPath}
        title="Aliases"
        subheader="A check with its arguments fixed, under a name of its own."
        note={
          <Explainer to="/modules/CheckHelpers" linkText="Open Check helpers">
            New aliases belong in Check helpers, which works the same without External scripts
            loaded. An alias takes $ARG1$, $ARG2$, ... from its caller whatever Allow arguments
            says, and never $ARGS$.
          </Explainer>
        }
        argumentsHelp={
          'Quote an argument that holds spaces: "warn=load > 80%". $ARG1$, $ARG2$, ... take the caller\'s arguments.'
        }
      />
      {sections.size > 0 && (
        <Explainer>
          {[...sections].join(", ")} {sections.size === 1 ? "is" : "are"} written as a section;{" "}
          <a
            href="#"
            onClick={(e) => {
              e.preventDefault();
              onExpertMode();
            }}
          >
            edit in expert mode
          </a>
          .
        </Explainer>
      )}
    </Stack>
  );
}
