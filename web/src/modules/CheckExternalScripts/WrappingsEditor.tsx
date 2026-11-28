// Wrappings: per extension, the command line a wrapped script runs with.
// ps1, vbs and bat are built in; storing one replaces it.
import { useGetSettingsQuery } from "../../api/api.ts";
import KeyValueCard from "../kit/KeyValueCard.tsx";
import ValueField from "../kit/ValueField.tsx";
import { EXT_WRAPPINGS } from "./ext.ts";
import { readWrappings } from "./wrapped.ts";

export default function WrappingsEditor() {
  const { data: stored } = useGetSettingsQuery();
  return (
    <KeyValueCard
      collectionPath={EXT_WRAPPINGS}
      title="Wrappings"
      subheader="How a wrapped script runs, by its extension: %SCRIPT% is the file, %ARGS% the arguments it is given, $ARGS&quot;$ the caller's."
      noun="wrapping"
      keyLabel="Extension"
      keyHelp="Without the dot, as the file names spell it: ps1, py, sh."
      valueLabel="Runs"
      rows={readWrappings(stored).map((w) => ({
        key: w.ext,
        value: w.line,
        builtIn: w.builtIn && !w.overridden,
        chip: w.overridden
          ? { label: "replaces the built-in" }
          : w.builtIn
            ? { label: "built in" }
            : undefined,
      }))}
      problem={(_, value) =>
        !value.includes("%SCRIPT%") ? "A wrapping needs %SCRIPT% where the file goes." : null
      }
      renderValue={(value, onChange) => (
        <ValueField
          label="Command line"
          kind="text"
          value={value}
          onChange={onChange}
          help='python "scripts\%SCRIPT%" %ARGS% $ARGS"$'
        />
      )}
    />
  );
}
