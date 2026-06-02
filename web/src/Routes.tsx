import Dashboard from "./pages/Dashboard.tsx";
import { BrowserRouter, Route, Routes } from "react-router";
import MainPage from "./pages/MainPage.tsx";
import Modules from "./pages/Modules.tsx";
import { useAuthentication } from "./common/hooks/auth.ts";
import Login from "./pages/Login.tsx";
import Logs from "./pages/Logs.tsx";
import Queries from "./pages/Queries.tsx";
import Query from "./pages/Query.tsx";
import Module from "./pages/Module.tsx";
import Settings from "./pages/Settings.tsx";
import Metrics from "./pages/Metrics.tsx";
import Events from "./pages/Events.tsx";
import About from "./pages/About.tsx";

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
          <Route index={true} element={<Dashboard />} />
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
          <Route path={"events"} element={<Events />} />
          <Route path={"about"} element={<About />} />
        </Route>
      </Routes>
    </BrowserRouter>
  );
}
