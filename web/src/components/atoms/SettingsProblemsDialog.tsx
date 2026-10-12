import {
  Box,
  Chip,
  CircularProgress,
  Dialog,
  DialogActions,
  DialogContent,
  DialogTitle,
  FormControlLabel,
  IconButton,
  Stack,
  Switch,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Typography,
} from "@mui/material";
import Button from "@mui/material/Button";
import CloseIcon from "@mui/icons-material/Close";
import { useState } from "react";
import { SettingsProblem, useGetSettingsProblemsQuery } from "../../api/api.ts";

interface Props {
  open: boolean;
  onClose: () => void;
}

function location(p: SettingsProblem) {
  // An orphan key has no section: it sits above the first header.
  const section = p.path ? `[${p.path}]` : "(before the first section)";
  return p.key ? `${section} ${p.key}` : section;
}

export default function SettingsProblemsDialog({ open, onClose }: Props) {
  const { data, isFetching, isError, refetch } = useGetSettingsProblemsQuery(undefined, { skip: !open });
  const [showInfo, setShowInfo] = useState(false);

  const problems = data?.problems ?? [];
  const warnings = data?.warnings ?? 0;
  const infos = problems.length - warnings;
  const shown = showInfo ? problems : problems.filter((p) => p.severity === "warning");

  return (
    <Dialog open={open} onClose={onClose} maxWidth="lg" fullWidth>
      <DialogTitle sx={{ display: "flex", alignItems: "center", gap: 1 }}>
        Configuration problems
        <Box sx={{ flexGrow: 1 }} />
        {isFetching && <CircularProgress size={18} />}
        <IconButton size="small" onClick={onClose} aria-label="close">
          <CloseIcon fontSize="small" />
        </IconButton>
      </DialogTitle>
      <DialogContent dividers>
        <Stack direction="column" spacing={1}>
          {isError && (
            <Typography variant="body2" color="error">
              Failed to check the configuration.
            </Typography>
          )}
          {data?.restricted && (
            <Typography variant="body2" color="text.secondary">
              Checking the configuration needs the settings.get permission, which your role does not have.
            </Typography>
          )}
          {data && !data.restricted && (
            <Stack direction="row" spacing={2} sx={{ alignItems: "center" }}>
              <Typography variant="body2" color="text.secondary" sx={{ flexGrow: 1 }}>
                {warnings === 0
                  ? "Nothing in the saved configuration is certainly wrong."
                  : `${warnings} entr${warnings === 1 ? "y is" : "ies are"} ignored because nothing reads ${warnings === 1 ? "it" : "them"}.`}{" "}
                Entries marked info are not used by any loaded module; they may belong to a module that is not enabled.
              </Typography>
              <FormControlLabel
                control={<Switch size="small" checked={showInfo} onChange={(e) => setShowInfo(e.target.checked)} />}
                label={`Show info (${infos})`}
              />
            </Stack>
          )}
          {shown.length > 0 && (
            <TableContainer>
              <Table size="small">
                <TableHead>
                  <TableRow>
                    <TableCell>Severity</TableCell>
                    <TableCell>Location</TableCell>
                    <TableCell>Problem</TableCell>
                  </TableRow>
                </TableHead>
                <TableBody>
                  {shown.map((p, idx) => (
                    <TableRow key={`${p.source}/${p.path}/${p.key}/${idx}`} hover>
                      <TableCell>
                        <Chip size="small" color={p.severity === "warning" ? "warning" : "default"} label={p.severity} />
                      </TableCell>
                      <TableCell>
                        <Box component="span" sx={{ fontFamily: "monospace", wordBreak: "break-all" }}>
                          {location(p)}
                        </Box>
                        <Typography variant="caption" component="div" color="text.secondary" sx={{ wordBreak: "break-all" }}>
                          {p.source}
                        </Typography>
                      </TableCell>
                      <TableCell>{p.message}</TableCell>
                    </TableRow>
                  ))}
                </TableBody>
              </Table>
            </TableContainer>
          )}
        </Stack>
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
