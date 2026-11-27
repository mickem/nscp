// Picks a web role's grants: everything, or grants by area, with the area's
// wildcard where it has one. Grants the picker does not know are kept.
import {
  Box,
  Checkbox,
  Chip,
  FormControlLabel,
  Stack,
  Switch,
  TextField,
  Tooltip,
  Typography,
} from "@mui/material";
import { useState } from "react";
import { GRANT_AREAS, grants, holds, otherGrants } from "./grants.ts";

interface Props {
  value: string[];
  onChange: (next: string[]) => void;
  disabled?: boolean;
}

export default function GrantPicker({ value, onChange, disabled }: Props) {
  const [custom, setCustom] = useState("");
  const everything = value.includes("*");
  const toggle = (grant: string, on: boolean) =>
    onChange(on ? [...value.filter((g) => g !== grant), grant] : value.filter((g) => g !== grant));
  // A wildcard covers only the grants below its prefix (`queries.*` is not
  // `aliases.list`, which sits in the same area).
  const covers = (area: (typeof GRANT_AREAS)[number], id: string) =>
    area.wildcard !== undefined && grants(area.wildcard, id);
  const setWildcard = (area: (typeof GRANT_AREAS)[number], on: boolean) => {
    // The wildcard replaces the single grants it covers.
    const rest = value.filter((g) => !covers(area, g) && g !== area.wildcard);
    onChange(on && area.wildcard ? [...rest, area.wildcard] : rest);
  };
  const others = otherGrants(value);

  return (
    <Stack spacing={1.5}>
      <FormControlLabel
        control={
          <Switch
            size="small"
            checked={everything}
            onChange={(e) => toggle("*", e.target.checked)}
            disabled={disabled}
          />
        }
        label="Everything (*)"
      />
      {!everything &&
        GRANT_AREAS.map((area) => {
          const wild = area.wildcard !== undefined && value.includes(area.wildcard);
          return (
            <Box key={area.id}>
              <Stack direction="row" spacing={1} sx={{ alignItems: "center" }}>
                <Typography variant="subtitle2">{area.label}</Typography>
                {area.wildcard && (
                  <FormControlLabel
                    control={
                      <Checkbox
                        size="small"
                        checked={wild}
                        onChange={(e) => setWildcard(area, e.target.checked)}
                        disabled={disabled}
                      />
                    }
                    label={
                      <Typography variant="caption" color="text.secondary">
                        all ({area.wildcard})
                      </Typography>
                    }
                  />
                )}
              </Stack>
              <Box sx={{ display: "flex", flexWrap: "wrap", columnGap: 2, pl: 1 }}>
                {area.grants.map((g) => (
                  <Tooltip key={g.id} title={g.id} placement="top-start" describeChild>
                    <FormControlLabel
                      control={
                        <Checkbox
                          size="small"
                          checked={holds(value, g.id)}
                          onChange={(e) => toggle(g.id, e.target.checked)}
                          disabled={disabled || (wild && covers(area, g.id))}
                        />
                      }
                      label={
                        <Typography variant="body2" color={g.powerful ? "warning.main" : undefined}>
                          {g.label}
                        </Typography>
                      }
                    />
                  </Tooltip>
                ))}
              </Box>
            </Box>
          );
        })}
      {!everything && (
        <Box>
          <Typography variant="subtitle2">Other grants</Typography>
          <Box sx={{ display: "flex", flexWrap: "wrap", gap: 1, my: 1 }}>
            {others.map((g) => (
              <Chip
                key={g}
                label={g}
                size="small"
                onDelete={disabled ? undefined : () => toggle(g, false)}
              />
            ))}
          </Box>
          <TextField
            size="small"
            variant="standard"
            label="Add a grant by name"
            value={custom}
            onChange={(e) => setCustom(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === "Enter" && custom.trim()) {
                e.preventDefault();
                toggle(custom.trim(), true);
                setCustom("");
              }
            }}
            helperText="Press Enter to add. A * segment grants everything below it."
            disabled={disabled}
          />
        </Box>
      )}
    </Stack>
  );
}
