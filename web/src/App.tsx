import { store } from "./store/store";
import { Provider } from "react-redux";
import Router from "./Routes.tsx";
import { CssBaseline } from "@mui/material";
import { createTheme, ThemeProvider } from "@mui/material/styles";

const theme = createTheme({
  colorSchemes: { dark: true },
  palette: {
    primary: {
      main: "#1565C0",
    },
    secondary: {
      main: "#00838F",
    },
  },
  shape: {
    borderRadius: 8,
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
