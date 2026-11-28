// Whether the agent holds changes its configuration file does not have yet,
// and the step that stores and applies them. A page's Save (and every list
// dialog) writes to the running agent only; this is the second step, the
// same as the "Save and reload" of the banner at the top of the app.
import { useState } from "react";
import { useGetSettingsStatusQuery, useSettingsCommandMutation } from "../../api/api.ts";

export function useStoreSettings() {
  const { data: status } = useGetSettingsStatusQuery();
  const [settingsCommand] = useSettingsCommandMutation();
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const store = async () => {
    setBusy(true);
    setError(null);
    try {
      await settingsCommand({ command: "save" }).unwrap();
      await settingsCommand({ command: "reload" }).unwrap();
    } catch (e) {
      setError(e instanceof Error ? e.message : JSON.stringify(e));
    } finally {
      setBusy(false);
    }
  };
  return { unstored: status?.has_changed ?? false, store, busy, error };
}

export const UNSTORED_TEXT = "Changed on the agent, not yet in its configuration file.";
