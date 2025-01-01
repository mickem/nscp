import { store } from "./store/store";
import { Provider } from "react-redux";
import Router from "./Routes.tsx";
import { CssBaseline } from "@mui/material";

export default function App() {
  return (
    <>
      <CssBaseline />
      <Provider store={store}>
        <Router />
      </Provider>
    </>
  );
}
