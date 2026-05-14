import { SettingsDescription, useGetSettingsDescriptionsQuery } from "../api/api.ts";
import { Stack } from "@mui/material";
import SettingsList from "./SettingsList.tsx";
import SettingsCollection from "./SettingsCollection.tsx";
import SettingsKeyValueCollection, {
  KEY_VALUE_COLLECTION_PATHS,
} from "./SettingsKeyValueCollection.tsx";
import NrpeServerWizard from "./NrpeServerWizard.tsx";
import { ReactElement, useMemo } from "react";

// Module-specific setup wizards that should render above the regular
// settings sections. Keyed by the module id we get in props.
const MODULE_WIZARDS: Record<string, () => ReactElement> = {
  NRPEServer: () => <NrpeServerWizard />,
};

interface Props {
  // The (already module-filtered, non-template) settings used for the flat list.
  settings: SettingsDescription[];
  // Module id — used to find collections owned by this module in the full settings list.
  moduleId: string;
  // Forwarded to SettingsCollection so advanced fields (debug, perf config…)
  // appear inside the per-instance editor when the module page toggle is on.
  showAdvanced?: boolean;
}

// Legacy collection roots that should never render via the new editor —
// they're either superseded by other config or have non-object semantics.
const LEGACY_COLLECTION_PATHS = new Set<string>([
  "/settings/system/windows/real-time/checks",
]);

// Collection roots are path-only entries (key === "") explicitly flagged
// by the backend with `is_object: true` (e.g. "PDH Counters", "External scripts").
// Paths registered as flat key/value collections are handled by their own
// widget instead, so we filter them out here even if the backend tags them
// as objects.
const KEY_VALUE_PATHS_SET = new Set<string>(KEY_VALUE_COLLECTION_PATHS);

function findCollectionRoots(all: SettingsDescription[], moduleId: string): SettingsDescription[] {
  return all.filter(
    (s) =>
      s.key === "" &&
      s.is_object &&
      s.plugins.includes(moduleId) &&
      !LEGACY_COLLECTION_PATHS.has(s.path) &&
      !KEY_VALUE_PATHS_SET.has(s.path),
  );
}

// Collections whose visibility depends on a sibling toggle being on. Keeps
// the editor focused: if real-time event-log polling is off, the per-filter
// editor below it is dead config and just adds noise.
interface CollectionGate {
  collectionPath: string;
  gatePath: string;
  gateKey: string;
}
const GATED_COLLECTIONS: CollectionGate[] = [
  {
    collectionPath: "/settings/eventlog/real-time/filters",
    gatePath: "/settings/eventlog/real-time",
    gateKey: "enabled",
  },
  {
    collectionPath: "/settings/logfile/real-time/checks",
    gatePath: "/settings/logfile/real-time",
    gateKey: "enabled",
  },
];

function isBoolTrue(value: string | undefined): boolean {
  const v = (value ?? "").toLowerCase();
  return v === "true" || v === "1";
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
    return KEY_VALUE_COLLECTION_PATHS.filter((p) => pathsInModule.has(p));
  }, [settings]);

  // Apply gating rules: a gated collection only renders when its gate setting
  // is on. Hidden collections are still excluded from the flat list (their
  // child keys aren't relevant when the parent is off either).
  const visibleCollectionPaths = useMemo(() => {
    if (!allDescriptions) return collectionPaths;
    return collectionPaths.filter((p) => {
      const gate = GATED_COLLECTIONS.find((g) => g.collectionPath === p);
      if (!gate) return true;
      const entry = allDescriptions.find(
        (s) => s.path === gate.gatePath && s.key === gate.gateKey,
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

  const renderWizard = MODULE_WIZARDS[moduleId];

  if (
    !renderWizard &&
    !hasFlatKeys &&
    visibleCollectionPaths.length === 0 &&
    kvPaths.length === 0
  ) {
    return null;
  }

  return (
    <Stack spacing={3}>
      {renderWizard && renderWizard()}
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
