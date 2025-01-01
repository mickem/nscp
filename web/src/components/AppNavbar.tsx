import AppBar from "@mui/material/AppBar";
import Stack from "@mui/material/Stack";
import Typography from "@mui/material/Typography";
import { IconButton, Toolbar } from "@mui/material";
import MenuIcon from "@mui/icons-material/Menu";

interface Props {
  handleDrawerToggle: () => void;
}

export default function AppNavbar({ handleDrawerToggle }: Props) {
  return (
    <AppBar
      position="fixed"
      sx={{
        zIndex: (theme) => theme.zIndex.drawer + 1,
      }}
    >
      <Toolbar>
        <Stack direction="row" spacing={1} sx={{ justifyContent: "center", mr: "auto" }}>
          <IconButton
            color="inherit"
            aria-label="open drawer"
            edge="start"
            onClick={handleDrawerToggle}
            sx={{ mr: 2, display: { sm: "none" } }}
          >
            <MenuIcon />
          </IconButton>
          <Typography variant="h4" component="h1">
            NSClient++
          </Typography>
        </Stack>
      </Toolbar>
    </AppBar>
  );
}
