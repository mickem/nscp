// Picks the host name a client reports itself as: one of the `auto` forms,
// or a name of its own built from placeholders.
import {
  Button,
  FormControl,
  FormControlLabel,
  Menu,
  MenuItem,
  Radio,
  RadioGroup,
  Stack,
  TextField,
  Typography,
} from "@mui/material";
import { useState } from "react";
import { HOSTNAME_FORMS, HOSTNAME_PLACEHOLDERS } from "./hostname.ts";

interface Props {
  value: string;
  onChange: (value: string) => void;
  disabled?: boolean;
}

const CUSTOM = "\u0000custom";

export default function HostnameField({ value, onChange, disabled }: Props) {
  const isForm = HOSTNAME_FORMS.some((f) => f.value === value);
  // Remember the custom text while an auto form is picked, to switch back.
  const [custom, setCustom] = useState(isForm ? "${host_lc}" : value);
  const [menu, setMenu] = useState<HTMLElement | null>(null);

  return (
    <FormControl component="fieldset" disabled={disabled} fullWidth>
      <RadioGroup
        value={isForm ? value : CUSTOM}
        onChange={(e) => onChange(e.target.value === CUSTOM ? custom : e.target.value)}
      >
        {HOSTNAME_FORMS.map((f) => (
          <FormControlLabel
            key={f.value}
            value={f.value}
            control={<Radio size="small" />}
            label={
              <Typography variant="body2">
                {f.label} <code>({f.value})</code>
              </Typography>
            }
          />
        ))}
        <FormControlLabel
          value={CUSTOM}
          control={<Radio size="small" />}
          label={<Typography variant="body2">A name of its own</Typography>}
        />
      </RadioGroup>
      {!isForm && (
        <Stack spacing={1} sx={{ pl: 4 }}>
          <TextField
            variant="standard"
            label="Host name"
            value={value}
            onChange={(e) => {
              setCustom(e.target.value);
              onChange(e.target.value);
            }}
            helperText="Fixed text, placeholders, or both: ${host_lc}.example.com"
            slotProps={{ htmlInput: { style: { fontFamily: "monospace" } } }}
          />
          <Stack direction="row">
            <Button size="small" onClick={(e) => setMenu(e.currentTarget)}>
              Insert a placeholder
            </Button>
          </Stack>
          <Menu anchorEl={menu} open={menu !== null} onClose={() => setMenu(null)}>
            {HOSTNAME_PLACEHOLDERS.map((p) => (
              <MenuItem
                key={p.token}
                onClick={() => {
                  setMenu(null);
                  const next = value + p.token;
                  setCustom(next);
                  onChange(next);
                }}
              >
                <Stack>
                  <Typography variant="body2" sx={{ fontFamily: "monospace" }}>
                    {p.token}
                  </Typography>
                  <Typography variant="caption" color="text.secondary">
                    {p.label}
                  </Typography>
                </Stack>
              </MenuItem>
            ))}
          </Menu>
        </Stack>
      )}
    </FormControl>
  );
}
