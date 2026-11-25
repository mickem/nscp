import { SettingsDescription, useGetSettingsDescriptionsQuery } from "../api/api.ts";
import { Stack } from "@mui/material";
import SettingsList from "./SettingsList.tsx";
import SettingsCollection from "./SettingsCollection.tsx";
import SettingsKeyValueCollection from "./SettingsKeyValueCollection.tsx";
import { useMemo } from "react";
import { isBoolTrue } from "../common/settings.ts";
import { getCollectionConfig, getModulePage } from "../modules/registry.ts";

interface Props {
  // The (already module-filtered, non-template) settings used for the flat list.
  settings: SettingsDescription[];
  // Module id — used to find collections owned by this module in the full settings list.
  moduleId: string;
  // Forwarded to SettingsCollection so advanced fields (debug, perf config…)
  // appear inside the per-instance editor when the module page toggle is on.
  showAdvanced?: boolean;
}

// Collection roots are path-only entries (key === "") explicitly flagged
// by the backend with `is_object: true` (e.g. "PDH Counters", "External scripts").
// A module page can declare one of them a key/value collection (rendered by
// its own widget) or hidden (superseded config), so those are left out here
// even though the backend tags them as objects.
function findCollectionRoots(all: SettingsDescription[], moduleId: string): SettingsDescription[] {
  return all.filter((s) => {
    if (s.key !== "" || !s.is_object || !s.plugins.includes(moduleId)) return false;
    const kind = getCollectionConfig(s.path)?.kind;
    return kind !== "hidden" && kind !== "keyValue";
  });
}

// The key/value collections a module page declares.
function keyValuePathsOf(moduleId: string): string[] {
  return Object.entries(getModulePage(moduleId)?.collections ?? {})
    .filter(([, c]) => c.kind === "keyValue")
    .map(([path]) => path);
}

export default function ModuleSettings({ settings, moduleId, showAdvanced = false }: Props) {
  const { data: allDescriptions } = useGetSettingsDescriptionsQuery();

  const collectionRoots = useMemo(
    () => (allDescriptions ? findCollectionRoots(allDescriptions, moduleId) : []),
    [allDescriptions, moduleId],
  );
  const collectionPaths = useMemo(() => collectionRoots.map((r) => r.path), [collectionRoots]);

  // KV-collection paths that this module owns. We detect ownership by the
  // path appearing in the module-filtered `settings` prop, which mirrors
  // how SettingsCollection scopes itself.
  const kvPaths = useMemo(() => {
    const pathsInModule = new Set(settings.map((s) => s.path));
    return keyValuePathsOf(moduleId).filter((p) => pathsInModule.has(p));
  }, [settings, moduleId]);

  // Apply the module page's gates: a gated collection only renders when its
  // gate setting is on. Hidden collections are still excluded from the flat list (their
  // child keys aren't relevant when the parent is off either).
  const visibleCollectionPaths = useMemo(() => {
    if (!allDescriptions) return collectionPaths;
    return collectionPaths.filter((p) => {
      const config = getCollectionConfig(p);
      const gate = config?.kind === "instances" ? config.gate : undefined;
      if (!gate) return true;
      const entry = allDescriptions.find(
        (s) => s.path === gate.path && s.key === gate.key,
      );
      return isBoolTrue(entry?.value || entry?.default_value);
    });
  }, [collectionPaths, allDescriptions]);

  // Drop collection-related entries from the flat list so they don't render twice.
  const flatSettings = useMemo(() => {
    const kvSet = new Set(kvPaths);
    const prefixes = collectionPaths.map((p) => p + "/");
    return settings.filter(
      (s) =>
        !collectionPaths.includes(s.path) &&
        !prefixes.some((p) => s.path.startsWith(p)) &&
        !kvSet.has(s.path),
    );
  }, [settings, collectionPaths, kvPaths]);

  // Only entries with a real key produce a visible row in SettingsList;
  // section-header entries (key === "") alone would render an empty card.
  const hasFlatKeys = flatSettings.some((s) => s.key !== "");

  const Wizard = getModulePage(moduleId)?.Wizard;

  if (
    !Wizard &&
    !hasFlatKeys &&
    visibleCollectionPaths.length === 0 &&
    kvPaths.length === 0
  ) {
    return null;
  }

  return (
    <Stack spacing={3}>
      {Wizard && <Wizard />}
      {hasFlatKeys && <SettingsList settings={flatSettings} flat />}
      {visibleCollectionPaths.map((path) => (
        <SettingsCollection
          key={path}
          collectionPath={path}
          settings={allDescriptions ?? []}
          showAdvanced={showAdvanced}
        />
      ))}
      {kvPaths.map((path) => (
        <SettingsKeyValueCollection
          key={path}
          collectionPath={path}
          descriptions={allDescriptions ?? []}
        />
      ))}
    </Stack>
  );
}
