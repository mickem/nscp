// The disk page: which paths callers may name, the growth-spots diagnostics,
// the volumes fact set and the background collector.
import { Checkbox, FormControlLabel, FormGroup, RadioGroup, Radio, Typography } from "@mui/material";
import { readStored } from "../kit/changes.ts";
import { tableIni } from "../kit/keys.ts";
import PanelShell, { AdvancedSection, KeyField, PanelSection } from "../kit/PanelShell.tsx";
import RiskBanner from "../kit/RiskBanner.tsx";
import type { FieldSpec } from "../kit/socketServer.ts";
import { usePanel } from "../kit/usePanel.ts";
import {
  accessRisk,
  COLLECTORS,
  disabledCollectors,
  disableValue,
  DISK_DIAGNOSTICS,
  DISK_FACTS,
  DISK_MODULE,
  DISK_PATH,
  DISK_TABLE,
  fileAccessOf,
} from "./disk.ts";

const ADVANCED = ["collection interval", "trend interval", "trend retention", "max collection errors", "disable"];

export default function DiskPage() {
  const panel = usePanel(DISK_MODULE, DISK_TABLE);
  const changes = panel.pending();
  const value = (key: string, path = DISK_PATH) => panel.value(path, key);
  const f = (s: FieldSpec, path = DISK_PATH) => (
    <KeyField
      key={`${path}/${s.key}`}
      panel={panel}
      path={path}
      k={s.key}
      label={s.label}
      kind={s.kind}
      help={s.help}
    />
  );
  const mode = fileAccessOf(value("file access"));
  const risk = accessRisk(mode, value("allowed files"));
  const growth = value("growth spots", DISK_DIAGNOSTICS) === "true";
  const disabled = disabledCollectors(value("disable"));
  const advancedSet = ADVANCED.filter((k) => readStored(panel.stored, DISK_PATH, k) !== undefined).length;

  return (
    <PanelShell
      title="Disks and files"
      subheader="Checks drive space, disk I/O and health, files and folders, shares and mounts."
      panel={panel}
      changes={changes}
      problem={null}
      ini={(stored, d) => tableIni(stored, DISK_MODULE, DISK_TABLE, d)}
    >
      <PanelSection title="Which paths callers may name">
        {risk && <RiskBanner risk={risk} />}
        <RadioGroup value={mode ?? ""} onChange={(e) => panel.set(DISK_PATH, "file access", e.target.value)}>
          <FormControlLabel value="any" control={<Radio size="small" />} label="Any path" />
          <FormControlLabel
            value="allowed"
            control={<Radio size="small" />}
            label="Paths under the ones listed, and the named paths below"
          />
          <FormControlLabel value="predefined" control={<Radio size="small" />} label="Only the named paths below" />
        </RadioGroup>
        {mode === "allowed" &&
          f({
            key: "allowed files",
            label: "Allowed paths",
            kind: "text",
            help: "Comma separated. A folder allows everything beneath it; * and ? do not cross a separator, ** does: C:\\Logs, /var/log/**/*.log",
          })}
        <Typography variant="caption" color="text.secondary">
          Applies to check_files, check_single_file, check_largest_files and check_disk_write. Drive and mount checks
          are not affected.
        </Typography>
      </PanelSection>

      <PanelSection title="Diagnostics">
        {f(
          {
            key: "growth spots",
            label: "Measure the growth spots",
            kind: "bool",
            help: "The size of the temp and log folders, page and swap files and shadow-copy storage, kept with a failed check. Paths and sizes leave the host with it.",
          },
          DISK_DIAGNOSTICS,
        )}
        {growth &&
          f(
            {
              key: "growth spots interval",
              label: "Measured every",
              kind: "text",
              help: "A duration, at least 10s: 10m.",
            },
            DISK_DIAGNOSTICS,
          )}
      </PanelSection>

      <PanelSection title="Facts">
        {f(
          {
            key: "storage.volumes",
            label: "Report the volumes",
            kind: "bool",
            help: "Mount point, device, filesystem, type, label and size of each volume, as storage.volumes. Not free space.",
          },
          DISK_FACTS,
        )}
      </PanelSection>

      <AdvancedSection count={advancedSet}>
        {f({
          key: "collection interval",
          label: "Sample disk I/O and free space every",
          kind: "text",
          help: "check_disk_io and check_disk_health report averages over one interval: 10s.",
        })}
        {f({
          key: "trend interval",
          label: "Keep a used-space sample every",
          kind: "text",
          help: "For check_drivesize's full_in and rate: 5m.",
        })}
        {f({
          key: "trend retention",
          label: "Keep used-space history for",
          kind: "text",
          help: "The longest trend window a check can use: 7d.",
        })}
        {f({
          key: "max collection errors",
          label: "Stop a collector after this many failures in a row",
          kind: "number",
          help: "Until the agent restarts; 0 retries forever.",
        })}
        <Typography variant="body2">Collectors switched off</Typography>
        <FormGroup>
          {COLLECTORS.map((c) => (
            <FormControlLabel
              key={c.id}
              control={
                <Checkbox
                  size="small"
                  checked={disabled.has(c.id)}
                  onChange={(e) => {
                    const next = new Set(disabled);
                    if (e.target.checked) next.add(c.id);
                    else next.delete(c.id);
                    panel.set(DISK_PATH, "disable", disableValue(next));
                  }}
                />
              }
              label={
                <span>
                  {c.label}
                  <Typography variant="caption" color="text.secondary" component="span">
                    {" "}
                    — {c.help}
                  </Typography>
                </span>
              }
            />
          ))}
        </FormGroup>
      </AdvancedSection>
    </PanelShell>
  );
}
