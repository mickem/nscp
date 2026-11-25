// What a module page tells the shared settings components about the module.
//
// Every piece of per-module knowledge in the settings UI (tab groups, preview
// columns, presets, dropdown choices, visibility rules, gates, wizards) lives
// in the module's own folder under `web/src/modules/<ModuleId>/` and reaches
// the shared parts through this shape. The shared parts never match on a
// settings path themselves.
//
// Paths here are exact settings paths ("/settings/system/unix/real-time/cpu"),
// never fragments: a module owns the paths it registers.
import type { ComponentType } from "react";
import type { SettingsDescription } from "../api/api.ts";

export interface EnumOption {
  value: string;
  label: string;
}

// Wizard-style tab of the per-instance edit dialog.
export interface FieldGroup {
  name: string;
  keys: string[];
}

// A pre-filled instance offered on a collection's Add button. Picking it
// materializes a new instance with every field already populated, so the user
// only has to confirm/tweak the name.
export interface InstanceTemplate {
  // Display name in the Add menu.
  name: string;
  // Optional sub-line in the menu and dialog hint.
  description?: string;
  // Suggested name for the new instance — pre-filled in the create dialog.
  defaultName: string;
  // Schema-key → value pairs to write under the new path. Empty values are
  // skipped (the backend interprets "" as a delete).
  fields: Record<string, string>;
}

// A pre-filled row offered on a key/value collection's Add button.
export interface KeyValueTemplate {
  // Menu label.
  label: string;
  // Optional secondary text shown in the menu.
  description?: string;
  // Suggested key (Add dialog will dedupe with _2/_3 suffixes on collision).
  defaultKey: string;
  // Initial value.
  defaultValue: string;
}

// The widget a key renders with, overriding the schema-driven default.
export type FieldWidget =
  // Single choice from a fixed list.
  | { kind: "select"; options: EnumOption[] }
  // Comma-separated combination of a fixed list, rendered as switches.
  | { kind: "multi"; options: string[] }
  // PDH counter picker backed by /v1/metadata/counters.
  | { kind: "counter" }
  // WEB role picker, populated from the roles defined on the server.
  | { kind: "role" };

// Per-key widgets, keyed by settings key.
export type FieldWidgets = Record<string, FieldWidget>;

export interface FieldVisibilityContext {
  // The fields of the instance being edited, with their current values.
  fields: SettingsDescription[];
  isTemplate: boolean;
}

// A path-only setting (key === "") whose children are object instances, each
// edited through the instance dialog.
export interface InstanceCollectionConfig {
  kind: "instances";
  // Tabs of the instance dialog. Keys in no group fall into an "Other" tab.
  fieldGroups?: FieldGroup[];
  // One column shown next to the instance name in the list.
  previewColumn?: { key: string; label: string };
  // Starting points on the Add button.
  presets?: InstanceTemplate[];
  // The collection is shown only while this bool setting is on.
  gate?: { path: string; key: string };
  // Widgets for the keys of every instance (the `default` template included).
  fields?: FieldWidgets;
  // Module-specific visibility rule, consulted after the generic ones.
  // Return undefined to fall through to the generic rules. Expert mode skips
  // it, so a field hidden here is still reachable there.
  isFieldVisible?: (key: string, ctx: FieldVisibilityContext) => boolean | undefined;
}

// A path whose entries are flat key/value pairs stored directly under it.
export interface KeyValueCollectionConfig {
  kind: "keyValue";
  presets?: KeyValueTemplate[];
}

// A collection root that must not render: superseded or non-object semantics.
export interface HiddenCollectionConfig {
  kind: "hidden";
}

export type CollectionConfig =
  InstanceCollectionConfig | KeyValueCollectionConfig | HiddenCollectionConfig;

// What a module's page is given by the module page around it.
export interface ModulePanelProps {
  // Switches the module page to expert mode, where every key can be edited.
  // A tailored control that cannot represent the stored keys links here.
  onExpertMode: () => void;
}

// What a module is for, so a setup wizard can map "which monitoring tools do
// you use" onto modules without a table of its own.
export interface ModulePurpose {
  // Monitoring tools the module talks to ("Nagios", "Icinga", ...).
  tools: string[];
  // The protocol, as the tools' users call it ("NRPE", "NSCA", ...).
  transport: string;
  // Active: the monitoring server asks the agent. Passive: the agent submits.
  direction: "active" | "passive";
  platforms: Array<"windows" | "linux" | "macos">;
}

export interface ModulePage {
  // Module id as the backend reports it ("NRPEServer", "CheckSystemUnix").
  id: string;
  // The tailored part of the page, rendered above the generic settings.
  Panel?: ComponentType<ModulePanelProps>;
  // Keys the panel edits, by exact section path. The generic list below the
  // panel leaves them out so nothing is edited in two places; expert mode
  // shows them as usual.
  handledKeys?: Record<string, string[]>;
  purpose?: ModulePurpose;
  // Collections the module owns, keyed by exact collection path.
  collections?: Record<string, CollectionConfig>;
  // Widgets for keys of plain sections, keyed by exact section path.
  sections?: Record<string, FieldWidgets>;
}
