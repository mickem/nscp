import {
  Box,
  Chip,
  CircularProgress,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
  IconButton,
  Stack,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Tooltip,
  Typography,
} from "@mui/material";
import Button from "@mui/material/Button";
import CloseIcon from "@mui/icons-material/Close";
import { SettingsDiffChangeType, SettingsDiffEntry, useGetSettingsDiffQuery } from "../../api/api.ts";

interface Props {
  open: boolean;
  onClose: () => void;
}

const CHANGE_LABELS: Record<SettingsDiffChangeType, { label: string; color: "success" | "error" | "warning" | "info" }> = {
  modified: { label: "modified", color: "warning" },
  added: { label: "added", color: "success" },
  removed: { label: "removed", color: "error" },
  path_added: { label: "section added", color: "success" },
  path_removed: { label: "section removed", color: "error" },
};

function renderValue(value: string, change: SettingsDiffChangeType, side: "old" | "new") {
  if (change === "added" && side === "old") return <em>(none)</em>;
  if (change === "removed" && side === "new") return <em>(none)</em>;
  if (change === "path_added" && side === "old") return <em>(none)</em>;
  if (change === "path_removed" && side === "new") return <em>(none)</em>;
  if (!value) return <em>(empty)</em>;
  return (
    <Box component="span" sx={{ fontFamily: "monospace", whiteSpace: "pre-wrap", wordBreak: "break-all" }}>
      {value}
    </Box>
  );
}

export default function SettingsDiffDialog({ open, onClose }: Props) {
  const { data, isFetching, refetch } = useGetSettingsDiffQuery(undefined, { skip: !open });

  const entries: SettingsDiffEntry[] = data?.entries ?? [];

  return (
    <Dialog open={open} onClose={onClose} maxWidth="lg" fullWidth>
      <DialogTitle sx={{ display: "flex", alignItems: "center", gap: 1 }}>
        Pending configuration changes
        <Box sx={{ flexGrow: 1 }} />
        {isFetching && <CircularProgress size={18} />}
        <IconButton size="small" onClick={onClose} aria-label="close">
          <CloseIcon fontSize="small" />
        </IconButton>
      </DialogTitle>
      <DialogContent dividers>
        {entries.length === 0 && !isFetching && (
          <Typography variant="body2" color="text.secondary">
            There are no unsaved changes.
          </Typography>
        )}
        {entries.length > 0 && (
          <Stack direction="column" spacing={1}>
            <Typography variant="body2" color="text.secondary">
              {entries.length} pending change{entries.length === 1 ? "" : "s"} between the in-memory and saved configuration.
            </Typography>
            <TableContainer>
              <Table size="small">
                <TableHead>
                  <TableRow>
                    <TableCell>Change</TableCell>
                    <TableCell>Path</TableCell>
                    <TableCell>Key</TableCell>
                    <TableCell>Saved value</TableCell>
                    <TableCell>New value</TableCell>
                  </TableRow>
                </TableHead>
                <TableBody>
                  {entries.map((e, idx) => {
                    const meta = CHANGE_LABELS[e.change_type] ?? CHANGE_LABELS.modified;
                    return (
                      <TableRow key={`${e.path}/${e.key}/${idx}`} hover>
                        <TableCell>
                          <Stack direction="row" spacing={1} alignItems="center">
                            <Chip size="small" color={meta.color} label={meta.label} />
                            {e.is_sensitive && (
                              <Tooltip title="Sensitive value, redacted">
                                <Chip size="small" variant="outlined" label="sensitive" />
                              </Tooltip>
                            )}
                          </Stack>
                        </TableCell>
                        <TableCell sx={{ fontFamily: "monospace" }}>{e.path}</TableCell>
                        <TableCell sx={{ fontFamily: "monospace" }}>{e.key || "—"}</TableCell>
                        <TableCell>{renderValue(e.old_value, e.change_type, "old")}</TableCell>
                        <TableCell>{renderValue(e.new_value, e.change_type, "new")}</TableCell>
                      </TableRow>
                    );
                  })}
                </TableBody>
              </Table>
            </TableContainer>
          </Stack>
        )}
      </DialogContent>
      <DialogActions>
        <Button onClick={() => refetch()}>Refresh</Button>
        <Button onClick={onClose} variant="contained">
          Close
        </Button>
      </DialogActions>
    </Dialog>
  );
}

