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
import Typography from "@mui/material/Typography";
import DangerousIcon from '@mui/icons-material/Dangerous';

const ICONS = {
  critical: <DangerousIcon color="error" />,
  error: <ErrorIcon color="error" />,
  warning: <WarningIcon color="warning" />,
  info: <InfoIcon color="primary" />,
  success: <DoneIcon color="success" />,
  debug: <AdbIcon color="secondary" />,
  unknown: <ErrorIcon color="error" />,
};

export default function Logs() {
  const [page, setPage] = useState(1);
  const dispatch = useAppDispatch();
  const [level, setLevel] = useState(["*"]);
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
          <Typography variant="body2">Filter:</Typography>
          <ToggleButtonGroup
            value={level}
            exclusive={false}
            onChange={(_e, level) => changeLevel(level)}
            aria-label="text alignment"
          >
            <ToggleButton value="debug">{ICONS.debug}</ToggleButton>
            <ToggleButton value="info">{ICONS.info}</ToggleButton>
            <ToggleButton value="warning">{ICONS.warning}</ToggleButton>
            <ToggleButton value="error">{ICONS.error}</ToggleButton>
            <ToggleButton value="critical">{ICONS.critical}</ToggleButton>
            <ToggleButton value="*">
              <CloseIcon /> Show all
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
