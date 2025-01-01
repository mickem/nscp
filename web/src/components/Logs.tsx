import Stack from "@mui/material/Stack";
import { LogRecord, nsclientApi, useGetLogsQuery } from "../api/api.ts";
import { Box, List, ListItem, ListItemIcon, ListItemText, Pagination } from "@mui/material";
import ErrorIcon from "@mui/icons-material/Error";
import WarningIcon from "@mui/icons-material/Warning";
import InfoIcon from "@mui/icons-material/Info";
import DoneIcon from "@mui/icons-material/Done";
import AdbIcon from "@mui/icons-material/Adb";
import { Toolbar } from "./atoms/Toolbar.tsx";
import { RefreshButton } from "./atoms/RefreshButton.tsx";
import { useAppDispatch } from "../store/store.ts";
import { Spacing } from "./atoms/Spacing.tsx";
import { useState } from "react";

const ICONS = {
  critical: <ErrorIcon color="error" />,
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

  const { data: logs } = useGetLogsQuery({ page: page });
  const getIcon = (log: LogRecord) => {
    return ICONS[log?.level] || ICONS.info;
  };
  const getKey = (log: LogRecord) => {
    return `${log.date}-${log.message}`;
  };
  const onRefresh = () => {
    dispatch(nsclientApi.util.invalidateTags(["Logs"]));
  };

  return (
    <Box sx={{ p: { sm: 3 } }}>
      <Stack direction="column">
        <Toolbar>
          <Spacing />
          <RefreshButton onRefresh={onRefresh} />
        </Toolbar>
        {logs?.count === 0 && <p>No logs found</p>}
        {logs?.content && (
          <>
            <List>
              {logs?.content?.map((log) => (
                <ListItem key={getKey(log)} dense>
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
