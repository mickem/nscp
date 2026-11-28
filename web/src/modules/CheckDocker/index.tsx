// Docker (modules/CheckDocker): the engine's local endpoint, and the fact
// sets. Only a local socket or named pipe is accepted (docker_endpoint.hpp):
// no tcp://, no TLS, no auth.
import { fixed, KeyTable, tableIni, tableKeys } from "../kit/keys.ts";
import PanelShell, { KeyField, PanelSection } from "../kit/PanelShell.tsx";
import { usePanel } from "../kit/usePanel.ts";
import type { ModulePage } from "../types.ts";
import { endpointProblem } from "./docker.ts";

const MODULE = "CheckDocker";
const PATH = "/settings/docker";
const FACTS = `${PATH}/facts`;
const TABLE: KeyTable = {
  // The default is per platform: the agent fills it in.
  [PATH]: {
    endpoint: { builtIn: fixed(""), computedDefault: true },
    timeout: { builtIn: fixed("10") },
  },
  [FACTS]: {
    docker: { builtIn: fixed("false") },
    "docker.containers": { builtIn: fixed("false") },
    "docker.images": { builtIn: fixed("false") },
  },
};

function DockerPage() {
  const panel = usePanel(MODULE, TABLE);
  const changes = panel.pending();
  const f = (
    k: string,
    label: string,
    kind: "text" | "number" | "bool",
    help: string,
    path = PATH,
  ) => <KeyField panel={panel} path={path} k={k} label={label} kind={kind} help={help} />;
  return (
    <PanelShell
      title="Docker"
      subheader="Checks containers and images on this host's Docker engine."
      panel={panel}
      changes={changes}
      problem={endpointProblem(panel.value(PATH, "endpoint"))}
      test={{
        query: "check_docker_info",
        what: "Asks the engine for its version and counts: proves the endpoint and the agent's access to it.",
      }}
      ini={(stored, d) => tableIni(stored, MODULE, TABLE, d)}
    >
      <PanelSection title="Engine">
        {f(
          "endpoint",
          "Endpoint",
          "text",
          "/var/run/docker.sock on Linux, \\\\.\\pipe\\docker_engine on Windows when empty. The agent's account needs access to it (root, or the docker group).",
        )}
        {f("timeout", "Timeout (seconds)", "number", "Per call to the engine.")}
      </PanelSection>
      <PanelSection title="Facts">
        {f(
          "docker",
          "Report the engine",
          "bool",
          "Version and the like, as the docker fact set.",
          FACTS,
        )}
        {f("docker.containers", "Report its containers", "bool", "As docker.containers.", FACTS)}
        {f("docker.images", "Report its images", "bool", "As docker.images.", FACTS)}
      </PanelSection>
    </PanelShell>
  );
}

const CheckDocker: ModulePage = {
  id: MODULE,
  Panel: DockerPage,
  handledKeys: tableKeys(TABLE),
};

export default CheckDocker;
