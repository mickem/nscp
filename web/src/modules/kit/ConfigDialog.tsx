// Shows a module's configuration as an INI fragment, to copy to another agent
// or into a fleet bundle.
import {
  Box,
  Button,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
  FormControlLabel,
  Switch,
  Typography,
} from "@mui/material";
import { useState } from "react";

interface Props {
  open: boolean;
  onClose: () => void;
  title: string;
  // The fragment, with or without the keys left at the agent's default.
  render: (includeDefaults: boolean) => string;
  // Said above the fragment, e.g. that it includes unsaved changes.
  note?: string;
}

export default function ConfigDialog({ open, onClose, title, render, note }: Props) {
  const [includeDefaults, setIncludeDefaults] = useState(true);
  const [copied, setCopied] = useState(false);
  const text = render(includeDefaults);
  const copy = async () => {
    try {
      await navigator.clipboard.writeText(text);
      setCopied(true);
    } catch {
      setCopied(false);
    }
  };
  return (
    <Dialog open={open} onClose={onClose} maxWidth="md" fullWidth>
      <DialogTitle>{title}</DialogTitle>
      <DialogContent>
        {note && (
          <Typography variant="body2" color="text.secondary" sx={{ mb: 1 }}>
            {note}
          </Typography>
        )}
        <FormControlLabel
          control={
            <Switch
              size="small"
              checked={includeDefaults}
              onChange={(e) => {
                setIncludeDefaults(e.target.checked);
                setCopied(false);
              }}
            />
          }
          label="Include values left at their default"
        />
        <Box
          component="pre"
          aria-label="INI fragment"
          sx={{
            fontFamily: "monospace",
            fontSize: 13,
            p: 1.5,
            mt: 1,
            border: 1,
            borderColor: "divider",
            borderRadius: 1,
            overflowX: "auto",
            whiteSpace: "pre",
          }}
        >
          {text}
        </Box>
      </DialogContent>
      <DialogActions>
        <Button onClick={() => void copy()}>{copied ? "Copied" : "Copy"}</Button>
        <Button onClick={onClose}>Close</Button>
      </DialogActions>
    </Dialog>
  );
}
