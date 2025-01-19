import Welcome from "./components/Welcome.tsx";
import { BrowserRouter, Route, Routes } from "react-router";
import MainPage from "./components/MainPage.tsx";
import Modules from "./components/Modules.tsx";
import { useAuthentication } from "./common/hooks/auth.ts";
import Login from "./components/Login.tsx";
import Logs from "./components/Logs.tsx";
import Queries from "./components/Queries.tsx";
import Query from "./components/Query.tsx";
import Module from "./components/Module.tsx";
import Settings from "./components/Settings.tsx";
import Metrics from "./components/Metrics.tsx";

export default function Router() {
  const { isAuthenticated } = useAuthentication();
  if (!isAuthenticated) {
    return (
      <BrowserRouter>
        <Routes>
          <Route path="*" element={<Login />} />
        </Routes>
      </BrowserRouter>
    );
  }
  return (
    <BrowserRouter>
      <Routes>
        <Route path="/" element={<MainPage />}>
          <Route index={true} element={<Welcome />} />
          <Route path={"modules"}>
            <Route path={":id"} element={<Module />} />
            <Route index={true} element={<Modules />} />
          </Route>
          <Route path={"logs"} element={<Logs />} />
          <Route path={"settings"} element={<Settings />} />
          <Route path={"queries"}>
            <Route path={":id"} element={<Query />} />
            <Route index={true} element={<Queries />} />
          </Route>
          <Route path={"metrics"} element={<Metrics />} />
        </Route>
      </Routes>
    </BrowserRouter>
  );
}
