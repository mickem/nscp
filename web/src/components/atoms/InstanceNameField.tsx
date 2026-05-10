import { IconButton, Stack, TextField, Tooltip, Typography } from "@mui/material";
import EditIcon from "@mui/icons-material/Edit";
import { useState } from "react";

interface Props {
  name: string;
  path: string;
  isDefaultTemplate: boolean;
  existingNames: string[];
  onRename: (oldPath: string, oldName: string, newName: string) => Promise<void>;
}

export default function InstanceNameField({
  name,
  path,
  isDefaultTemplate,
  existingNames,
  onRename,
}: Props) {
  const [editing, setEditing] = useState(false);
  const [draft, setDraft] = useState(name);
  const [busy, setBusy] = useState(false);

  if (isDefaultTemplate) {
    // The "default" template's name is structural — referenced by every
    // instance via `parent` — so renaming it is not allowed.
    return <Typography variant="h6">{name}</Typography>;
  }

  const trimmed = draft.trim();
  const isDuplicate = trimmed !== name && existingNames.includes(trimmed);
  const invalid = trimmed === "" || trimmed.includes("/") || isDuplicate;
  const helper = isDuplicate
    ? "Name already exists"
    : trimmed.includes("/")
      ? "Name cannot contain '/'"
      : "";

  const commit = async () => {
    if (invalid) {
      setDraft(name);
      setEditing(false);
      return;
    }
    if (trimmed === name) {
      setEditing(false);
      return;
    }
    setBusy(true);
    try {
      await onRename(path, name, trimmed);
    } finally {
      setBusy(false);
      setEditing(false);
    }
  };

  if (!editing) {
    return (
      <Stack direction="row" alignItems="center" spacing={0.5}>
        <Typography variant="h6">{name}</Typography>
        <Tooltip title="Rename" arrow>
          <IconButton
            size="small"
            onClick={() => {
              setDraft(name);
              setEditing(true);
            }}
          >
            <EditIcon fontSize="small" />
          </IconButton>
        </Tooltip>
      </Stack>
    );
  }

  return (
    <TextField
      autoFocus
      size="small"
      variant="standard"
      value={draft}
      error={invalid && trimmed !== ""}
      helperText={helper || undefined}
      onChange={(e) => setDraft(e.target.value)}
      onBlur={commit}
      disabled={busy}
      onKeyDown={(e) => {
        if (e.key === "Enter") {
          (e.target as HTMLInputElement).blur();
        } else if (e.key === "Escape") {
          setDraft(name);
          setEditing(false);
        }
      }}
      sx={{ minWidth: 180 }}
    />
  );
}
