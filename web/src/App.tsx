import { store } from "./store/store";
import { Provider } from "react-redux";
import Router from "./Routes.tsx";
import { CssBaseline } from "@mui/material";
import { createTheme, ThemeProvider } from "@mui/material/styles";

// Brand palette inspired by the classic NSClient++ green identity, with
// complementary accents to bring more life to the (otherwise quite monotone)
// dashboard.
const brandGreen = "#43A047"; // primary - NSClient signature green
const brandGreenDark = "#2E7D32";
const brandAccent = "#FF6F00"; // secondary - warm amber for contrast/CTAs
const brandTeal = "#00ACC1"; // info  - cool counterpoint
const brandRed = "#E53935";
const brandAmber = "#FFB300";

const theme = createTheme({
  colorSchemes: { dark: true },
  palette: {
    mode: "dark",
    primary: {
      main: brandGreen,
      dark: brandGreenDark,
      light: "#76D275",
      contrastText: "#FFFFFF",
    },
    secondary: {
      main: brandAccent,
      light: "#FFA040",
      dark: "#C43E00",
      contrastText: "#FFFFFF",
    },
    info: { main: brandTeal },
    success: { main: brandGreen },
    warning: { main: brandAmber },
    error: { main: brandRed },
    background: {
      default: "#121821",
      paper: "#1B2330",
    },
  },
  shape: {
    borderRadius: 8,
  },
  typography: {
    fontFamily: ["Inter", "Roboto", "Helvetica", "Arial", "sans-serif"].join(","),
    h4: { fontWeight: 600 },
    h5: { fontWeight: 500 },
  },
  components: {
    MuiAppBar: {
      defaultProps: { color: "primary" },
      styleOverrides: {
        colorPrimary: {
          backgroundImage: `linear-gradient(90deg, ${brandGreenDark} 0%, ${brandGreen} 60%, #66BB6A 100%)`,
          boxShadow: "0 2px 8px rgba(0,0,0,0.35)",
        },
      },
    },
    MuiDrawer: {
      styleOverrides: {
        paper: {
          backgroundImage: "linear-gradient(180deg, #1B2330 0%, #141A24 100%)",
          borderRight: "1px solid rgba(67,160,71,0.25)",
        },
      },
    },
    MuiPaper: {
      styleOverrides: {
        root: {
          backgroundImage: "none",
        },
      },
    },
    MuiCard: {
      styleOverrides: {
        root: {
          borderTop: `3px solid ${brandGreen}`,
          backgroundImage:
            "linear-gradient(180deg, rgba(67,160,71,0.06) 0%, rgba(0,0,0,0) 60%)",
        },
      },
    },
    MuiButton: {
      styleOverrides: {
        root: { textTransform: "none", fontWeight: 600 },
        containedPrimary: {
          boxShadow: "0 2px 6px rgba(67,160,71,0.35)",
        },
      },
    },
    MuiChip: {
      styleOverrides: {
        colorPrimary: { color: "#fff" },
      },
    },
    MuiLink: {
      styleOverrides: {
        root: { color: brandGreen, textDecorationColor: "rgba(67,160,71,0.5)" },
      },
    },
    MuiTab: {
      styleOverrides: {
        root: { textTransform: "none", fontWeight: 600 },
      },
    },
  },
});

export default function App() {
  return (
    <ThemeProvider theme={theme}>
      <CssBaseline />
      <Provider store={store}>
        <Router />
      </Provider>
    </ThemeProvider>
  );
}
