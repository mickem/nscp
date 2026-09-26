import {
  Card,
  CardContent,
  Divider,
  List,
  ListItem,
  ListItemText,
  ListSubheader,
  Stack,
  Switch,
  Tooltip,
  Typography,
} from "@mui/material";
import { useMemo, useState } from "react";
import { SettingsDescription, useGetSettingsDescriptionsQuery, useUpdateSettingsMutation } from "../api/api.ts";

/**
 * The fact sets this agent can collect, and the switch that turns each on.
 *
 * Facts are opt-in per set, and each switch lives in the settings of the
 * module that produces it — `[/settings/disk/facts] storage.volumes` for
 * CheckDisk, `[/settings/system/windows/facts] os` for CheckSystem, and the
 * core's own `[/settings/facts] agent`. That is a good rule for the agent (a
 * switch cannot exist without the module that honours it) and a bad one for
 * an operator, who would otherwise have to visit one module page per set to
 * find out what this host could be reporting and does not.
 *
 * So the widget finds the sets rather than listing them: every settings
 * section called `facts`, whichever module registered it, with its boolean
 * keys as the rows. A module added later needs nothing here — if it registers
 * `[/settings/<something>/facts] <set> = true`, its set appears.
 */

/** A settings section holding fact-set switches, and the switches in it. */
interface FactSection {
  path: string;
  /** Who registered it: the module name, or "Core" for the core's own set. */
  owner: string;
  keys: SettingsDescription[];
}

function isFactSwitch(entry: SettingsDescription): boolean {
  // The section, not the key, is what marks a fact set: the key is the set's
  // own dotted id (`software.installed`), which is not a name this can match
  // on. Only the bools are switches - the same section carries the core's
  // `interval` and `max size`, which are settings about facts rather than
  // sets of them.
  return entry.path.endsWith("/facts") && entry.key !== "" && entry.type === "bool";
}

/**
 * Group the fact switches by the section they live in, sections and keys both
 * in a stable order so the list does not reshuffle as settings are re-fetched.
 */
export function groupFactSections(descriptions: SettingsDescription[]): FactSection[] {
  const sections = new Map<string, FactSection>();
  for (const entry of descriptions.filter(isFactSwitch)) {
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

function isOn(entry: SettingsDescription): boolean {
  // An unset key reads as its default, exactly as the agent reads it.
  const value = (entry.value || entry.default_value || "").toLowerCase();
  return value === "true" || value === "1";
}

export default function FactSetsWidget() {
  const { data: descriptions } = useGetSettingsDescriptionsQuery();
  const [updateSettings] = useUpdateSettingsMutation();
  // Keyed by path and key: two switches can be in flight at once, and a
  // single "saving" flag would disable the whole list for each of them.
  const [pending, setPending] = useState<Record<string, boolean>>({});

  const sections = useMemo(() => groupFactSections(descriptions ?? []), [descriptions]);

  // Nothing loaded produces facts. Rendering an empty card would leave an
  // operator looking for the switch that is not there.
  if (sections.length === 0) return null;

  const toggle = async (entry: SettingsDescription, next: boolean) => {
    const id = `${entry.path}/${entry.key}`;
    setPending((state) => ({ ...state, [id]: true }));
    try {
      await updateSettings({ path: entry.path, key: entry.key, value: next ? "true" : "false" }).unwrap();
    } finally {
      setPending((state) => ({ ...state, [id]: false }));
    }
  };

  return (
    <Card variant="outlined">
      <CardContent>
        <Typography gutterBottom variant="h6" component="div">
          Fact sets
        </Typography>
        {/* Writing the setting is not collecting: the module re-reads which
            sets it produces when the configuration is reloaded. The unsaved
            configuration banner at the top of the page is how that is done,
            so this says what to expect rather than offering a second button
            for it. */}
        <Typography variant="body2" color="text.secondary">
          Nothing is collected until you turn a set on. A set starts (or stops) collecting when the configuration is
          saved and reloaded.
        </Typography>
        {/* One list per section rather than one list with headers in it: a
            section is a module's own settings, and nesting them into a single
            list means nesting <ul> inside <li>, which is where the stray
            bullets and the mis-drawn dividers come from. */}
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
                        // The set's id, which is what the switch is called
                        // everywhere else: the settings key, the document
                        // and the documentation.
                        slotProps={{ input: { "aria-label": entry.key } }}
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
      </CardContent>
    </Card>
  );
}
