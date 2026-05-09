import { SettingsDescription, useGetSettingsDescriptionsQuery } from "../api/api.ts";
import { Stack } from "@mui/material";
import SettingsList from "./SettingsList.tsx";
import SettingsCollection from "./SettingsCollection.tsx";
import { useMemo } from "react";

interface Props {
  // The (already module-filtered, non-template) settings used for the flat list.
  settings: SettingsDescription[];
  // Module id — used to find collections owned by this module in the full settings list.
  moduleId: string;
}

// Legacy collection roots that should never render via the new editor —
// they're either superseded by other config or have non-object semantics.
const LEGACY_COLLECTION_PATHS = new Set<string>([
  "/settings/system/windows/real-time/checks",
]);

// Collection roots are path-only entries (key === "") explicitly flagged
// by the backend with `is_object: true` (e.g. "PDH Counters", "External scripts").
function findCollectionRoots(all: SettingsDescription[], moduleId: string): SettingsDescription[] {
  return all.filter(
    (s) =>
      s.key === "" &&
      s.is_object &&
      s.plugins.includes(moduleId) &&
      !LEGACY_COLLECTION_PATHS.has(s.path),
  );
}

export default function ModuleSettings({ settings, moduleId }: Props) {
  const { data: allDescriptions } = useGetSettingsDescriptionsQuery();

  const collectionRoots = useMemo(
    () => (allDescriptions ? findCollectionRoots(allDescriptions, moduleId) : []),
    [allDescriptions, moduleId],
  );
  const collectionPaths = useMemo(() => collectionRoots.map((r) => r.path), [collectionRoots]);

  // Drop collection-related entries from the flat list so they don't render twice.
  const flatSettings = useMemo(() => {
    if (collectionPaths.length === 0) return settings;
    const prefixes = collectionPaths.map((p) => p + "/");
    return settings.filter(
      (s) => !collectionPaths.includes(s.path) && !prefixes.some((p) => s.path.startsWith(p)),
    );
  }, [settings, collectionPaths]);

  if ((!settings || settings.length === 0) && collectionPaths.length === 0) {
    return null;
  }

  return (
    <Stack spacing={3}>
      {collectionPaths.map((path) => (
        <SettingsCollection
          key={path}
          collectionPath={path}
          settings={allDescriptions ?? []}
        />
      ))}
      {flatSettings.length > 0 && <SettingsList settings={flatSettings} />}
    </Stack>
  );
}
