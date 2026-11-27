// Which check_nt commands the server answers: everything, or a pick of
// groups and single commands.
import {
  Box,
  Checkbox,
  FormControlLabel,
  FormGroup,
  Stack,
  Switch,
  Typography,
} from "@mui/material";
import NscpAlert from "../../components/atoms/NscpAlert.tsx";
import { ALL_COMMANDS, AllowSpec, COMMAND_GROUPS } from "./nsclient.ts";

interface Props {
  value: AllowSpec;
  onChange: (next: AllowSpec) => void;
  disabled?: boolean;
}

export default function AllowedCommandsPicker({ value, onChange, disabled }: Props) {
  const all = ALL_COMMANDS.every((c) => value.commands.has(c));
  const set = (commands: string[], on: boolean) => {
    const next = new Set(value.commands);
    commands.forEach((c) => (on ? next.add(c) : next.delete(c)));
    onChange({ ...value, commands: next });
  };
  return (
    <Stack spacing={1}>
      <Typography variant="subtitle2">Allowed commands</Typography>
      <FormControlLabel
        control={
          <Switch
            size="small"
            checked={all}
            onChange={(e) => set(ALL_COMMANDS, e.target.checked)}
            disabled={disabled}
          />
        }
        label="All commands (full check_nt compatibility)"
      />
      {!all && (
        <Box sx={{ pl: 1 }}>
          {COMMAND_GROUPS.map((g) => {
            const on = g.commands.filter((c) => value.commands.has(c));
            return (
              <Box key={g.id}>
                <FormControlLabel
                  control={
                    <Checkbox
                      size="small"
                      checked={on.length === g.commands.length}
                      indeterminate={on.length > 0 && on.length < g.commands.length}
                      onChange={(e) => set(g.commands, e.target.checked)}
                      disabled={disabled}
                    />
                  }
                  label={
                    <span>
                      {g.label}{" "}
                      <Typography component="span" variant="caption" color="text.secondary">
                        ({g.id})
                      </Typography>
                    </span>
                  }
                />
                {g.commands.length > 1 && (
                  <FormGroup row sx={{ pl: 4 }}>
                    {g.commands.map((c) => (
                      <FormControlLabel
                        key={c}
                        control={
                          <Checkbox
                            size="small"
                            checked={value.commands.has(c)}
                            onChange={(e) => set([c], e.target.checked)}
                            disabled={disabled}
                          />
                        }
                        label={<Typography variant="body2">{c}</Typography>}
                      />
                    ))}
                  </FormGroup>
                )}
              </Box>
            );
          })}
        </Box>
      )}
      {value.unknown.length > 0 && (
        <NscpAlert
          severity="info"
          text={`Also stored, and kept: ${value.unknown.join(", ")} - names the agent does not know and ignores.`}
        />
      )}
    </Stack>
  );
}
