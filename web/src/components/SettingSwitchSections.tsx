import { Divider, List, ListItem, ListItemText, ListSubheader, Stack, Switch, Tooltip, Typography } from "@mui/material";
import { useState } from "react";
import { SettingsDescription, useUpdateSettingsMutation } from "../api/api.ts";

/**
 * Boolean settings shown as switches, grouped by the settings section they
 * live in: what the fact sets and the diagnostics tables have in common. Each
 * is opt-in, and each switch lives in the settings of the module that honours
 * it, so a widget finds them by the name of their section rather than listing
 * them - a module added later needs nothing here.
 */

/** A settings section holding switches, and the switches in it. */
export interface SwitchSection {
  path: string;
  /** Who registered it: the module name, or "Core" for the core's own. */
  owner: string;
  keys: SettingsDescription[];
}

/**
 * Group the settings `isSwitch` picks by the section they live in, sections
 * and keys both in a stable order so the list does not reshuffle as settings
 * are re-fetched.
 */
export function groupSwitchSections(
  descriptions: SettingsDescription[],
  isSwitch: (entry: SettingsDescription) => boolean,
): SwitchSection[] {
  const sections = new Map<string, SwitchSection>();
  for (const entry of descriptions.filter(isSwitch)) {
    const section = sections.get(entry.path) ?? {
      path: entry.path,
      // The core registers its own section and is not a plugin, so it has no
      // name to report; every other section names the module that owns it.
      owner: entry.plugins.length > 0 ? entry.plugins.join(", ") : "Core",
      keys: [],
    };
    section.keys.push(entry);
    sections.set(entry.path, section);
  }
  return [...sections.values()]
    .map((section) => ({
      ...section,
      keys: [...section.keys].sort((a, b) => a.key.localeCompare(b.key)),
    }))
    .sort((a, b) => a.owner.localeCompare(b.owner) || a.path.localeCompare(b.path));
}

export function isOn(entry: SettingsDescription): boolean {
  // An unset key reads as its default, exactly as the agent reads it.
  const value = (entry.value || entry.default_value || "").toLowerCase();
  return value === "true" || value === "1";
}

interface SettingSwitchSectionsProps {
  sections: SwitchSection[];
  /**
   * The accessible name of a switch. The key alone by default; a widget whose
   * sections repeat a key (two modules with an `errors` table) qualifies it.
   */
  label?: (section: SwitchSection, entry: SettingsDescription) => string;
}

export default function SettingSwitchSections({ sections, label }: SettingSwitchSectionsProps) {
  const [updateSettings] = useUpdateSettingsMutation();
  // Keyed by path and key: two switches can be in flight at once, and a
  // single "saving" flag would disable the whole list for each of them.
  const [pending, setPending] = useState<Record<string, boolean>>({});

  const toggle = async (entry: SettingsDescription, next: boolean) => {
    const id = `${entry.path}/${entry.key}`;
    setPending((state) => ({ ...state, [id]: true }));
    try {
      await updateSettings({ path: entry.path, key: entry.key, value: next ? "true" : "false" }).unwrap();
    } finally {
      setPending((state) => ({ ...state, [id]: false }));
    }
  };

  // One list per section rather than one list with headers in it: a section
  // is a module's own settings, and nesting them into a single list means
  // nesting <ul> inside <li>, which is where the stray bullets and the
  // mis-drawn dividers come from.
  return (
    <Stack divider={<Divider />} sx={{ mt: 1 }}>
      {sections.map((section) => (
        <List
          key={section.path}
          dense
          disablePadding
          subheader={
            <ListSubheader disableGutters sx={{ bgcolor: "transparent", lineHeight: 2.5 }}>
              <Stack direction="row" spacing={1} sx={{ alignItems: "baseline" }}>
                <Typography variant="subtitle2" color="text.primary">
                  {section.owner}
                </Typography>
                <Typography variant="caption" color="text.secondary" sx={{ fontFamily: "monospace" }}>
                  {section.path}
                </Typography>
              </Stack>
            </ListSubheader>
          }
        >
          {section.keys.map((entry) => (
            <Tooltip
              key={entry.key}
              placement="top"
              title={
                <>
                  {entry.title !== "" && (
                    <Typography variant="caption" sx={{ fontWeight: "bold", display: "block" }}>
                      {entry.title}
                    </Typography>
                  )}
                  <Typography variant="caption">{entry.description}</Typography>
                </>
              }
            >
              <ListItem
                disableGutters
                secondaryAction={
                  <Switch
                    edge="end"
                    size="small"
                    checked={isOn(entry)}
                    disabled={pending[`${entry.path}/${entry.key}`] === true}
                    // The key by default, which is what the switch is called
                    // everywhere else: the configuration and the
                    // documentation.
                    slotProps={{ input: { "aria-label": label ? label(section, entry) : entry.key } }}
                    onChange={(event) => void toggle(entry, event.target.checked)}
                  />
                }
              >
                <ListItemText primary={entry.key} />
              </ListItem>
            </Tooltip>
          ))}
        </List>
      ))}
    </Stack>
  );
}
