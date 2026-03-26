import Stack from "@mui/material/Stack";
import { LogRecord, nsclientApi, useGetLogsQuery, useResetLogStatusMutation } from "../api/api.ts";
import {
  Box,
  List,
  ListItem,
  ListItemIcon,
  ListItemText,
  Pagination,
  ToggleButton,
  ToggleButtonGroup,
} from "@mui/material";
import ErrorIcon from "@mui/icons-material/Error";
import WarningIcon from "@mui/icons-material/Warning";
import InfoIcon from "@mui/icons-material/Info";
import DoneIcon from "@mui/icons-material/Done";
import AdbIcon from "@mui/icons-material/Adb";
import { Toolbar } from "./atoms/Toolbar.tsx";
import { RefreshButton } from "./atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import { Spacing } from "./atoms/Spacing.tsx";
import { useMemo, useState } from "react";
import CloseIcon from "@mui/icons-material/Close";
import Button from "@mui/material/Button";
import DangerousIcon from '@mui/icons-material/Dangerous';

const ICONS = {
  critical: <DangerousIcon fontSize="small" color="error" />,
  error: <ErrorIcon fontSize="small" color="error" />,
  warning: <WarningIcon fontSize="small" color="warning" />,
  info: <InfoIcon fontSize="small" color="primary" />,
  success: <DoneIcon fontSize="small" color="success" />,
  debug: <AdbIcon fontSize="small" color="secondary" />,
  unknown: <ErrorIcon fontSize="small" color="error" />,
};

export default function Logs() {
  const [page, setPage] = useState(1);
  const dispatch = useAppDispatch();
  const [level, setLevel] = useState(["info", "warning", "error", "critical"]);
  const [resetStatus] = useResetLogStatusMutation();

  const levelToString = useMemo(() => {
    if (Array.isArray(level)) {
      if (level.length === 0) return "*";
      return level.join(",");
    }
    return level;
  }, [level]);

  const { data: logs } = useGetLogsQuery({ page: page, level: levelToString });
  const getIcon = (log: LogRecord) => {
    return ICONS[log?.level] || ICONS.info;
  };
  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Logs", "LogStatus"]));
  };

  const clear = async () => {
    await resetStatus().unwrap();
  };

  const changeLevel = (level: string[]) => {
    if (level.includes("*")) {
      level = ["debug", "info", "warning", "error", "critical"];
    }
    setLevel(level)
    setPage(1);
  };

  return (
    <Box>
      <Stack direction="column">
        <Toolbar>
          <ToggleButtonGroup
            size="small"
            value={level}
            exclusive={false}
            onChange={(_e, level) => changeLevel(level)}
            aria-label="log level filter"
            sx={{ "& .MuiToggleButton-root": { px: 1, py: 0.5 } }}
          >
            <ToggleButton value="debug">{ICONS.debug}</ToggleButton>
            <ToggleButton value="info">{ICONS.info}</ToggleButton>
            <ToggleButton value="warning">{ICONS.warning}</ToggleButton>
            <ToggleButton value="error">{ICONS.error}</ToggleButton>
            <ToggleButton value="critical">{ICONS.critical}</ToggleButton>
            <ToggleButton value="*">
              <CloseIcon fontSize="small" /> Show all
            </ToggleButton>
          </ToggleButtonGroup>

          <Spacing />
          <Button size="small" onClick={clear}>
            Clear log
          </Button>
          <RefreshButton onRefresh={onRefresh} />
        </Toolbar>
        {logs?.count === 0 && <p>No logs found</p>}
        {logs?.content && (
          <>
            <List>
              {logs?.content?.map((log, index) => (
                <ListItem key={index} dense>
                  <ListItemIcon>{getIcon(log)}</ListItemIcon>
                  <ListItemText primary={log.message} secondary={log.date} />
                </ListItem>
              ))}
            </List>
            <Pagination count={+logs.pages} onChange={(_e, value) => setPage(value)} />
          </>
        )}
      </Stack>
    </Box>
  );
}
