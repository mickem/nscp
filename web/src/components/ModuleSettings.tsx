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
  // Expert mode: the generic schema-driven form of every key the module owns.
  // Advanced keys are shown, and the module page's layout (wizard, tabs,
  // preview columns, presets, visibility rules, gates) is left out, so
  // nothing the tailored page does not anticipate is out of reach. Widgets
  // that only change how a value is typed (dropdowns, pickers) still apply.
  expert?: boolean;
  // Switches the page to expert mode; handed to the module's panel.
  onExpertMode?: () => void;
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

const NO_OP = () => {};

export default function ModuleSettings({
  settings,
  moduleId,
  expert = false,
  onExpertMode = NO_OP,
}: Props) {
  const page = getModulePage(moduleId);
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
  // child keys aren't relevant when the parent is off either). Expert mode
  // shows every collection.
  const visibleCollectionPaths = useMemo(() => {
    if (!allDescriptions || expert) return collectionPaths;
    return collectionPaths.filter((p) => {
      const config = getCollectionConfig(p);
      const gate = config?.kind === "instances" ? config.gate : undefined;
      if (!gate) return true;
      const entry = allDescriptions.find((s) => s.path === gate.path && s.key === gate.key);
      return isBoolTrue(entry?.value || entry?.default_value);
    });
  }, [collectionPaths, allDescriptions, expert]);

  // Drop collection-related entries from the flat list so they don't render
  // twice, and the keys the module's panel edits (outside expert mode).
  const flatSettings = useMemo(() => {
    const kvSet = new Set(kvPaths);
    const prefixes = collectionPaths.map((p) => p + "/");
    const handled = expert ? undefined : page?.handledKeys;
    return settings.filter(
      (s) =>
        !collectionPaths.includes(s.path) &&
        !prefixes.some((p) => s.path.startsWith(p)) &&
        !kvSet.has(s.path) &&
        !handled?.[s.path]?.includes(s.key),
    );
  }, [settings, collectionPaths, kvPaths, expert, page]);

  // Only entries with a real key produce a visible row in SettingsList;
  // section-header entries (key === "") alone would render an empty card.
  const hasFlatKeys = flatSettings.some((s) => s.key !== "");

  const Panel = expert ? undefined : page?.Panel;

  if (!Panel && !hasFlatKeys && visibleCollectionPaths.length === 0 && kvPaths.length === 0) {
    return null;
  }

  return (
    <Stack spacing={3}>
      {Panel && <Panel onExpertMode={onExpertMode} />}
      {hasFlatKeys && <SettingsList settings={flatSettings} flat />}
      {visibleCollectionPaths.map((path) => (
        <SettingsCollection
          key={path}
          collectionPath={path}
          settings={allDescriptions ?? []}
          expert={expert}
        />
      ))}
      {kvPaths.map((path) => (
        <SettingsKeyValueCollection
          key={path}
          collectionPath={path}
          descriptions={allDescriptions ?? []}
          expert={expert}
        />
      ))}
    </Stack>
  );
}
