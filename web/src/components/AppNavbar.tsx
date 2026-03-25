import AppBar from "@mui/material/AppBar";
import Typography from "@mui/material/Typography";
import { Badge, Box, IconButton, Toolbar, Tooltip } from "@mui/material";
import MenuIcon from "@mui/icons-material/Menu";
import ErrorOutlineIcon from "@mui/icons-material/ErrorOutline";
import { useGetInfoQuery, useGetLogStatusQuery } from "../api/api.ts";
import AccountCircle from "@mui/icons-material/AccountCircle";
import Menu from "@mui/material/Menu";
import MenuItem from "@mui/material/MenuItem";
import { useAuthentication } from "../common/hooks/auth";
import { useState, MouseEvent } from "react";
import { useNavigate } from "react-router";
import { useAppSelector } from "../store/store.ts";

interface Props {
  handleDrawerToggle: () => void;
}

export default function AppNavbar({ handleDrawerToggle }: Props) {
  const { data: info } = useGetInfoQuery();
  const version = info?.version || "unknown";
  const { logout } = useAuthentication();
  const navigate = useNavigate();
  const refreshRate = useAppSelector((state) => state.dashboard.refreshRate);
  const { data: logStatus } = useGetLogStatusQuery(undefined, {
    pollingInterval: refreshRate || undefined,
  });
  const errors = logStatus?.errors || 0;
  const lastError = logStatus?.last_error || "No errors found";

  const [anchorEl, setAnchorEl] = useState<null | HTMLElement>(null);
  const handleMenu = (event: MouseEvent<HTMLElement>) => {
    setAnchorEl(event.currentTarget);
  };
  const handleClose = () => {
    setAnchorEl(null);
  };
  const handleLogout = async () => {
    await logout();
    handleClose();
  };
  return (
    <Box sx={{ flexGrow: 1 }}>
      <AppBar
        position="fixed"
        sx={{
          zIndex: (theme) => theme.zIndex.drawer + 1,
        }}
      >
        <Toolbar>
          <IconButton
            color="inherit"
            aria-label="open drawer"
            edge="start"
            onClick={handleDrawerToggle}
            sx={{ mr: 2, display: { sm: "none" } }}
          >
            <MenuIcon />
          </IconButton>
          <Typography variant="h4" component="div" sx={{ paddingRight: 3 }}>
            NSClient++
          </Typography>
          <Typography
            variant="h5"
            color="textDisabled"
            component="div"
            sx={{ flexGrow: 1, display: { xs: "none", sm: "block" } }}
          >
            {version}
          </Typography>
          {errors > 0 && (
            <Tooltip title={lastError}>
              <IconButton color="error" onClick={() => navigate("/logs")}>
                <Badge badgeContent={errors} color="error">
                  <ErrorOutlineIcon />
                </Badge>
              </IconButton>
            </Tooltip>
          )}
          <Box sx={{ flexGrow: 0 }}>
            <IconButton
              size="large"
              aria-label="account of current user"
              aria-controls="menu-appbar"
              aria-haspopup="true"
              onClick={handleMenu}
              color="inherit"
            >
              <AccountCircle />
            </IconButton>
            <Menu
              id="menu-appbar"
              anchorEl={anchorEl}
              anchorOrigin={{ vertical: "top", horizontal: "right" }}
              keepMounted
              transformOrigin={{ vertical: "top", horizontal: "right" }}
              open={Boolean(anchorEl)}
              onClose={handleClose}
            >
              <MenuItem onClick={handleLogout}>Logout</MenuItem>
            </Menu>
          </Box>
        </Toolbar>
      </AppBar>
    </Box>
  );
}
