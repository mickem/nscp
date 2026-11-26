// One choice that writes several keys, and reads them back into a mode.
//
// The selector never decides what the stored keys mean: the page detects the
// mode and passes it in. When the stored keys match none of the modes the
// page passes CUSTOM, and the selector says so, offers no fix, and points to
// expert mode. Keeping "Custom" selected leaves the keys exactly as they are,
// which is why a page must never snap such a configuration to the nearest mode
// on load.
import {
  Button,
  FormControl,
  FormControlLabel,
  Radio,
  RadioGroup,
  Stack,
  Typography,
} from "@mui/material";

import { CUSTOM, Custom, ModeOption } from "./modes.ts";

interface Props<M extends string> {
  label: string;
  options: ModeOption<M>[];
  // The selected mode, or CUSTOM.
  value: M | Custom;
  // Whether the stored keys matched none of the modes. The Custom option is
  // only offered then, so the reader can go back to "leave it alone".
  storedIsCustom: boolean;
  onChange: (value: M | Custom) => void;
  // Switches the module page to expert mode, where every key can be edited.
  onExpertMode?: () => void;
  disabled?: boolean;
}

export default function ModeSelector<M extends string>({
  label,
  options,
  value,
  storedIsCustom,
  onChange,
  onExpertMode,
  disabled,
}: Props<M>) {
  return (
    <FormControl disabled={disabled} component="fieldset">
      <Typography component="legend" variant="subtitle2" sx={{ mb: 0.5 }}>
        {label}
      </Typography>
      <RadioGroup value={value} onChange={(e) => onChange(e.target.value as M | Custom)}>
        {options.map((o) => (
          <FormControlLabel
            key={o.value}
            value={o.value}
            control={<Radio size="small" />}
            label={
              <Stack>
                <Typography variant="body2">{o.label}</Typography>
                {o.description && (
                  <Typography variant="caption" color="text.secondary">
                    {o.description}
                  </Typography>
                )}
              </Stack>
            }
          />
        ))}
        {storedIsCustom && (
          <FormControlLabel
            value={CUSTOM}
            control={<Radio size="small" />}
            label={
              <Stack>
                <Typography variant="body2">Custom (keep the current settings)</Typography>
                <Typography variant="caption" color="text.secondary">
                  The stored settings match none of the modes above, so they are left as they are.
                </Typography>
              </Stack>
            }
          />
        )}
      </RadioGroup>
      {storedIsCustom && onExpertMode && (
        <Stack direction="row">
          <Button size="small" onClick={onExpertMode}>
            Edit them in expert mode
          </Button>
        </Stack>
      )}
    </FormControl>
  );
}
