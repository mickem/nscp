import { SettingsDescription, useUpdateSettingsMutation } from "../../api/api.ts";
import {
  Box,
  Chip,
  Collapse,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
  Divider,
  IconButton,
  Stack,
  Typography,
} from "@mui/material";
import Button from "@mui/material/Button";
import { useEffect, useMemo, useState } from "react";
import SettingsDialogValue from "./SettingsDialogValue.tsx";
import ExpandMoreIcon from "@mui/icons-material/ExpandMore";
import CloseIcon from "@mui/icons-material/Close";
import RestoreIcon from "@mui/icons-material/RestoreOutlined";

interface Params {
  onClose: () => void;
  path: string;
  keyName: string;
  value: string;
  description?: SettingsDescription;
}

export default function SettingsDialog({ onClose, path, keyName, value, description }: Params) {
  const key = keyName;
  const descriptionText = description?.description || "No description available.";
  const plugins = description?.plugins || [];
  const title = description?.title || keyName;
  const defaultValue = description?.default_value ?? "";
  const type = description?.type || "string";

  const [newValue, setNewValue] = useState(value);
  const [showDetails, setShowDetails] = useState(false);

  const [saveSettings] = useUpdateSettingsMutation();

  useEffect(() => {
    setNewValue(value);
  }, [value]);

  const dirty = useMemo(() => newValue !== value, [newValue, value]);
  const isDefault = newValue === defaultValue;

  const onHandleSave = async () => {
    await saveSettings({ path, key: keyName, value: newValue }).unwrap();
    onClose();
  };

  return (
    <Dialog
      open={true}
      onClose={onClose}
      maxWidth="sm"
      fullWidth
      slotProps={{ paper: { sx: { borderRadius: 2 } } }}
    >
      <DialogTitle sx={{ pr: 6, pb: 1 }}>
        <Stack direction="row" spacing={1} alignItems="center" sx={{ mb: 0.5 }} flexWrap="wrap">
          <Typography variant="caption" color="text.secondary" sx={{ fontFamily: "monospace" }}>
            {path}
          </Typography>
          <Typography variant="caption" color="text.disabled">
            /
          </Typography>
          <Typography variant="caption" sx={{ fontFamily: "monospace", fontWeight: 600 }}>
            {key}
          </Typography>
          {type !== "string" && (
            <Chip
              label={type}
              size="small"
              variant="outlined"
              sx={{ height: 18, fontSize: 10, textTransform: "uppercase", letterSpacing: 0.5 }}
            />
          )}
        </Stack>
        <Typography variant="h6">{title}</Typography>
        <IconButton
          aria-label="close"
          onClick={onClose}
          sx={{ position: "absolute", right: 8, top: 8, color: (t) => t.palette.grey[500] }}
        >
          <CloseIcon />
        </IconButton>
      </DialogTitle>
      <DialogContent dividers sx={{ py: 2.5 }}>
        <Typography variant="body2" color="text.secondary" sx={{ mb: 2.5 }}>
          {descriptionText}
        </Typography>

        <SettingsDialogValue value={newValue} description={description} onChange={setNewValue} />

        <Stack direction="row" spacing={1} alignItems="center" sx={{ mt: 1.5 }}>
          <Button
            size="small"
            startIcon={<RestoreIcon />}
            disabled={isDefault}
            onClick={() => setNewValue(defaultValue)}
          >
            Restore default
          </Button>
          {defaultValue && (
            <Typography variant="caption" color="text.secondary" sx={{ fontFamily: "monospace" }}>
              default: {defaultValue}
            </Typography>
          )}
        </Stack>

        <Divider sx={{ my: 2.5 }} />

        <Stack
          direction="row"
          spacing={1}
          alignItems="center"
          onClick={() => setShowDetails((s) => !s)}
          sx={{ cursor: "pointer", userSelect: "none", color: "text.secondary" }}
        >
          <ExpandMoreIcon
            fontSize="small"
            sx={{
              transition: "transform 120ms ease",
              transform: showDetails ? "rotate(0deg)" : "rotate(-90deg)",
            }}
          />
          <Typography variant="body2">Details</Typography>
        </Stack>
        <Collapse in={showDetails}>
          <Box sx={{ mt: 1.5, pl: 3.5 }}>
            <DetailRow label="Path" value={path} mono />
            <DetailRow label="Key" value={key} mono />
            <DetailRow label="Type" value={type} mono />
            <DetailRow label="Default" value={defaultValue || "(none)"} mono />
            {plugins.length > 0 && (
              <Stack direction="row" spacing={1} sx={{ mt: 1 }} alignItems="flex-start">
                <Typography
                  variant="caption"
                  color="text.secondary"
                  sx={{ minWidth: 70, mt: 0.4 }}
                >
                  Plugins
                </Typography>
                <Stack direction="row" spacing={0.5} flexWrap="wrap" useFlexGap>
                  {plugins.map((p) => (
                    <Chip key={p} label={p} size="small" variant="outlined" />
                  ))}
                </Stack>
              </Stack>
            )}
          </Box>
        </Collapse>
      </DialogContent>
      <DialogActions sx={{ px: 3, py: 1.5 }}>
        <Button onClick={onClose}>Cancel</Button>
        <Button onClick={onHandleSave} variant="contained" disabled={!dirty} autoFocus>
          {dirty ? "Save change" : "Up to date"}
        </Button>
      </DialogActions>
    </Dialog>
  );
}

function DetailRow({ label, value, mono }: { label: string; value: string; mono?: boolean }) {
  return (
    <Stack direction="row" spacing={1} sx={{ mb: 0.5 }}>
      <Typography variant="caption" color="text.secondary" sx={{ minWidth: 70 }}>
        {label}
      </Typography>
      <Typography
        variant="caption"
        sx={{ fontFamily: mono ? "monospace" : undefined, wordBreak: "break-all" }}
      >
        {value}
      </Typography>
    </Stack>
  );
}
