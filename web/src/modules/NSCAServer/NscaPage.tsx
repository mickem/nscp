// The NSCA server page: where passive results arrive, how they are
// protected, and how they are read.
import { Button, Stack } from "@mui/material";
import AllowedHostsSection from "../kit/AllowedHostsSection.tsx";
import { readStored } from "../kit/changes.ts";
import PanelShell, { AdvancedSection, KeyField, PanelSection } from "../kit/PanelShell.tsx";
import RiskBanner from "../kit/RiskBanner.tsx";
import { CORE_ADVANCED, FieldSpec } from "../kit/socketServer.ts";
import { usePanel } from "../kit/usePanel.ts";
import type { ModulePanelProps } from "../types.ts";
import {
  encryptionOptions,
  encryptionRisk,
  isSecureEncryption,
  NSCA_MODULE,
  NSCA_PATH,
  NSCA_TABLE,
  nscaIni,
  passwordProblem,
  tlsOn,
} from "./nsca.ts";

const ADVANCED: FieldSpec[] = [
  {
    key: "payload length",
    label: "Payload length",
    kind: "number",
    help: "Bytes per packet. Must match send_nsca's compiled-in value; 512 for standard NSCA.",
  },
  ...CORE_ADVANCED,
];

export default function NscaPage({ onExpertMode }: ModulePanelProps) {
  const panel = usePanel(NSCA_MODULE, NSCA_TABLE);
  const changes = panel.pending();
  const value = (key: string) => panel.value(NSCA_PATH, key);
  const f = (s: FieldSpec) => (
    <KeyField
      key={s.key}
      panel={panel}
      path={NSCA_PATH}
      k={s.key}
      label={s.label}
      kind={s.kind}
      help={s.help}
    />
  );

  const encryption = value("encryption");
  const usesTls = tlsOn(panel.stored);
  const encRisk = encryptionRisk(encryption, usesTls);
  const problem = passwordProblem(encryption, value("password"));
  const hosts = panel.get(NSCA_PATH, "allowed hosts");
  const advancedSet = ADVANCED.filter(
    (a) => readStored(panel.stored, NSCA_PATH, a.key) !== undefined,
  ).length;

  return (
    <PanelShell
      title="NSCA server"
      subheader="Receives passive check results from send_nsca and other NSCA clients, and hands them on to the inbox channel."
      panel={panel}
      changes={changes}
      problem={problem}
      ini={nscaIni}
    >
      <PanelSection title="Connection">
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          <Stack sx={{ minWidth: 120 }}>{f({ key: "port", label: "Port", kind: "number" })}</Stack>
          {f({ key: "timeout", label: "Timeout (seconds)", kind: "number" })}
        </Stack>
        <AllowedHostsSection
          value={hosts.value}
          scope={hosts.scope ?? "default"}
          onChange={(v) => panel.set(NSCA_PATH, "allowed hosts", v)}
          onScopeChange={(sc) => panel.setScope(NSCA_PATH, "allowed hosts", sc)}
          disabled={panel.busy}
        />
      </PanelSection>

      <PanelSection title="Encryption">
        {encRisk && <RiskBanner risk={encRisk} />}
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          {f({
            key: "encryption",
            label: "Encryption",
            kind: { options: encryptionOptions(encryption) },
            help: isSecureEncryption(encryption)
              ? "The NSCA payload cipher. Every sending host has to use the same one; an older cipher, or none, is set in expert mode."
              : `${encryption} was set outside this page, which offers only the secure ciphers. Every sending host has to use the same one.`,
          })}
          {f({
            key: "password",
            label: "Password",
            kind: "secret",
            help: "The shared key every sending host uses. Not taken from [/settings/default].",
          })}
        </Stack>
        {usesTls && (
          <>
            <RiskBanner
              risk={{
                severity: "warning",
                title: "TLS is on for this server",
                reason:
                  "NSCA protects results with its own encryption, and standard send_nsca cannot connect over TLS. TLS is set in expert mode.",
              }}
            />
            <Stack direction="row">
              <Button size="small" onClick={onExpertMode}>
                Open expert mode
              </Button>
            </Stack>
          </>
        )}
      </PanelSection>

      <PanelSection title="Results">
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          {f({
            key: "inbox",
            label: "Inbox channel",
            kind: "text",
            help: "The channel received results are posted on.",
          })}
          {f({
            key: "timezone",
            label: "Timezone",
            kind: "text",
            help: "Of the timestamp in the packet: utc as the protocol says, or local for a legacy sender. Both ends must agree.",
          })}
        </Stack>
        {f({
          key: "performance data",
          label: "Keep performance data",
          kind: "bool",
          help: "Turn off to drop the performance data of every received result.",
        })}
      </PanelSection>

      <AdvancedSection count={advancedSet}>{ADVANCED.map(f)}</AdvancedSection>
    </PanelShell>
  );
}
