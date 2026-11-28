// The WMI page: which queries callers may run.
import { FormControlLabel, Radio, RadioGroup } from "@mui/material";
import { tableIni } from "../kit/keys.ts";
import PanelShell, { KeyField, PanelSection } from "../kit/PanelShell.tsx";
import RiskBanner from "../kit/RiskBanner.tsx";
import { usePanel } from "../kit/usePanel.ts";
import { accessRisk, queryAccessOf, WMI_MODULE, WMI_PATH, WMI_TABLE } from "./wmi.ts";

export default function WmiPage() {
  const panel = usePanel(WMI_MODULE, WMI_TABLE);
  const changes = panel.pending();
  const value = (key: string) => panel.value(WMI_PATH, key);
  const mode = queryAccessOf(value("query access"));
  const risk = accessRisk(mode, value("allowed classes"));
  const field = (k: string, label: string, help: string) => (
    <KeyField panel={panel} path={WMI_PATH} k={k} label={label} kind="text" help={help} />
  );

  return (
    <PanelShell
      title="WMI"
      subheader="Runs WMI queries as checks, on this host or remote ones (Windows)."
      panel={panel}
      changes={changes}
      problem={null}
      ini={(stored, d) => tableIni(stored, WMI_MODULE, WMI_TABLE, d)}
    >
      <PanelSection title="What callers may query">
        {risk && <RiskBanner risk={risk} />}
        <RadioGroup
          value={mode ?? ""}
          onChange={(e) => panel.set(WMI_PATH, "query access", e.target.value)}
        >
          <FormControlLabel value="any" control={<Radio size="small" />} label="Any query" />
          <FormControlLabel
            value="allowed"
            control={<Radio size="small" />}
            label="Plain SELECTs from the classes listed, in the namespaces listed"
          />
          <FormControlLabel
            value="predefined"
            control={<Radio size="small" />}
            label="Only the named queries below"
          />
        </RadioGroup>
        {mode === "allowed" &&
          field(
            "allowed classes",
            "Allowed classes",
            "Comma separated; * and ? match any text or character, case does not matter: Win32_Service, Win32_Perf*",
          )}
        {mode !== "any" &&
          field(
            "allowed namespaces",
            "Allowed namespaces",
            "Comma separated, as above; only root\\cimv2 when empty. A namespace naming a remote machine is always refused.",
          )}
      </PanelSection>
    </PanelShell>
  );
}
