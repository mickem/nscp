import AppBar from "@mui/material/AppBar";
import Typography from "@mui/material/Typography";
import { Box, IconButton, Toolbar } from "@mui/material";
import MenuIcon from "@mui/icons-material/Menu";
import LogStatusIcon from "./LogStatusIcon.tsx";

interface Props {
  handleDrawerToggle: () => void;
}

export default function AppNavbar({ handleDrawerToggle }: Props) {
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
          <Typography variant="h4" component="div" sx={{ flexGrow: 1 }}>
            NSClient++
          </Typography>
          <LogStatusIcon />
        </Toolbar>
      </AppBar>
    </Box>
  );
}
