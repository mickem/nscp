import { SettingsDescription, useGetSettingsDescriptionsQuery } from "../api/api.ts";
import { Box, Stack } from "@mui/material";
import SettingsList from "./SettingsList.tsx";
import SettingsCollection from "./SettingsCollection.tsx";
import SettingsKeyValueCollection from "./SettingsKeyValueCollection.tsx";
import { useMemo, useState } from "react";
import { PageFrameContext } from "../modules/kit/pageFrame.ts";
import ListsBar from "../modules/kit/ListsBar.tsx";
import { StagedOp, StagingContext } from "../modules/kit/staging.ts";
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

  // A module's own page replaces the generic list outside expert mode: it
  // covers every key of its sections, and listing them again would edit each
  // in two places. Its collections (users, entries, ...) still follow it.
  const Panel = expert ? undefined : page?.Panel;
  const [saveBar, setSaveBar] = useState<HTMLElement | null>(null);
  // The rows the page's lists have queued until its Save.
  const [ops, setOps] = useState<StagedOp[]>([]);
  const staging = useMemo(
    () => ({
      ops,
      stage: (next: StagedOp[]) => setOps((o) => [...o, ...next]),
      clear: () => setOps([]),
    }),
    [ops],
  );
  const showFlat = hasFlatKeys && !Panel;

  if (!Panel && !showFlat && visibleCollectionPaths.length === 0 && kvPaths.length === 0) {
    return null;
  }

  const lists = visibleCollectionPaths.length + kvPaths.length;
  return (
    <PageFrameContext.Provider
      value={{ saveBar, collectionPaths: [...visibleCollectionPaths, ...kvPaths] }}
    >
      <StagingContext.Provider value={expert ? null : staging}>
      <Stack spacing={3}>
        {Panel && <Panel onExpertMode={onExpertMode} />}
        {showFlat && <SettingsList settings={flatSettings} flat />}
        {visibleCollectionPaths.map((path) => {
          const config = getCollectionConfig(path);
          const Editor = !expert && config?.kind === "instances" ? config.Editor : undefined;
          return Editor ? (
            <Editor key={path} collectionPath={path} onExpertMode={onExpertMode} />
          ) : (
            <SettingsCollection
              key={path}
              collectionPath={path}
              settings={allDescriptions ?? []}
              expert={expert}
            />
          );
        })}
        {kvPaths.map((path) => {
          const config = getCollectionConfig(path);
          const Editor = !expert && config?.kind === "keyValue" ? config.Editor : undefined;
          return Editor ? (
            <Editor key={path} collectionPath={path} descriptions={allDescriptions ?? []} />
          ) : (
            <SettingsKeyValueCollection
              key={path}
              collectionPath={path}
              descriptions={allDescriptions ?? []}
              expert={expert}
            />
          );
        })}
        {/* The panel's save bar: pinned to the bottom of the window while it has
          unsaved changes, below the lists. */}
        <Box ref={setSaveBar} sx={{ position: "sticky", bottom: 0, zIndex: 2 }}>
          {/* A page that is only lists still saves what they queue. */}
          {!Panel && !expert && page && lists > 0 && (
            <ListsBar moduleId={moduleId} paths={[...visibleCollectionPaths, ...kvPaths]} />
          )}
        </Box>
      </Stack>
      </StagingContext.Provider>
    </PageFrameContext.Provider>
  );
}
