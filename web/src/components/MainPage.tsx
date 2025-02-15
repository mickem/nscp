import { Box, Toolbar } from "@mui/material";
import SideBar from "./SideBar.tsx";
import AppNavbar from "./AppNavbar.tsx";
import { Outlet } from "react-router";
import { useState } from "react";

const drawerWidth = 240;

export default function MainPage() {
  const [mobileOpen, setMobileOpen] = useState(false);
  const [isClosing, setIsClosing] = useState(false);

  const handleDrawerClose = () => {
    setIsClosing(true);
    setMobileOpen(false);
  };
  const handleDrawerTransitionEnd = () => {
    setIsClosing(false);
  };

  const handleDrawerToggle = () => {
    if (!isClosing) {
      setMobileOpen(!mobileOpen);
    }
  };
  return (
    <Box sx={{ display: "flex" }}>
      <AppNavbar handleDrawerToggle={handleDrawerToggle} />
      <SideBar mobileOpen={mobileOpen} onClose={handleDrawerClose} onTransitionEnd={handleDrawerTransitionEnd} />

      <Box
        component="main"
        sx={{
          flexGrow: 1,
          p: { sm: 3 },
          width: { sm: `calc(100% - ${drawerWidth}px)` },
        }}
      >
        {" "}
        <Toolbar />
        <Outlet />
      </Box>
    </Box>
  );
}
