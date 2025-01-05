import { CircularProgress, IconButton, Menu, MenuItem, Tooltip } from "@mui/material";
import { nsclientApi, useGetLogStatusQuery, useResetLogStatusMutation } from "../api/api.ts";
import DoneIcon from "@mui/icons-material/Done";
import ErrorIcon from "@mui/icons-material/Error";
import Typography from "@mui/material/Typography";
import React, { useEffect, useState } from "react";
import { useNavigate } from "react-router";
import { useAppDispatch } from "../store/store.ts";

export default function LogStatusIcon() {
  const { data: logStatus, isFetching } = useGetLogStatusQuery();
  const [resetStatus] = useResetLogStatusMutation();
  const dispatch = useAppDispatch();
  const navigate = useNavigate();
  const errors = logStatus?.errors || 0;
  const lastError = logStatus?.last_error || "";

  useEffect(() => {
    const onRefresh = () => dispatch(nsclientApi.util.invalidateTags(["LogStatus", "Logs"]));
    const interval = setInterval(onRefresh, 5_000);
    return () => {
      clearInterval(interval);
    };
  }, [dispatch]);

  const [anchor, setAnchor] = useState<null | HTMLElement>(null);

  const handleOpenUserMenu = (event: React.MouseEvent<HTMLElement>) => {
    setAnchor(event.currentTarget);
  };

  const closeMenu = () => {
    setAnchor(null);
  };

  const viewLogs = () => {
    closeMenu();
    navigate("/logs");
  };
  const reset = () => {
    closeMenu();
    resetStatus();
  };

  const icon = isFetching ? (
    <CircularProgress size="2em" />
  ) : errors === 0 ? (
    <DoneIcon color="success" />
  ) : (
    <ErrorIcon color="error" />
  );

  return (
    <>
      <Tooltip disableFocusListener title={lastError}>
        <IconButton onClick={handleOpenUserMenu}>{icon}</IconButton>
      </Tooltip>
      <Menu
        sx={{ mt: "45px" }}
        id="menu-appbar"
        anchorEl={anchor}
        anchorOrigin={{
          vertical: "top",
          horizontal: "right",
        }}
        keepMounted
        transformOrigin={{
          vertical: "top",
          horizontal: "right",
        }}
        open={Boolean(anchor)}
        onClose={closeMenu}
      >
        <MenuItem onClick={viewLogs}>
          <Typography sx={{ textAlign: "center" }}>View logs</Typography>
        </MenuItem>
        <MenuItem onClick={reset}>
          <Typography sx={{ textAlign: "center" }}>Reset status</Typography>
        </MenuItem>
      </Menu>
    </>
  );
}
