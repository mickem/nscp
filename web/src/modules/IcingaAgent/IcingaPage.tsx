// The Icinga agent page: the parent this agent connects to, how it comes to
// trust it, and the checks the parent may run.
import { Stack } from "@mui/material";
import { readStored } from "../kit/changes.ts";
import PanelShell, { AdvancedSection, KeyField, PanelSection } from "../kit/PanelShell.tsx";
import RiskBanner from "../kit/RiskBanner.tsx";
import type { FieldSpec } from "../kit/socketServer.ts";
import { HTTP_TLS_VERSIONS, tlsVersionRisk } from "../kit/tls.ts";
import { usePanel } from "../kit/usePanel.ts";
import {
  endpointProblem,
  firstUseRisk,
  ICINGA_MODULE,
  ICINGA_PATH,
  ICINGA_TABLE,
  icingaIni,
  offNote,
} from "./icinga.ts";

const ADVANCED: FieldSpec[] = [
  {
    key: "certificate directory",
    label: "Certificate directory",
    kind: "text",
    help: "Where the agent keeps its certificate, key and the Icinga CA; generated on first start.",
  },
  {
    key: "certificate request interval",
    label: "Certificate renewal interval (seconds)",
    kind: "number",
    help: "How often to ask for a renewed certificate on an open connection; 0 asks only on connect.",
  },
  {
    key: "tls version",
    label: "TLS version",
    kind: { options: HTTP_TLS_VERSIONS },
    help: "A + allows that version and newer; 1.2+ is what Icinga 2 accepts out of the box.",
  },
  {
    key: "allowed ciphers",
    label: "Allowed ciphers",
    kind: "text",
    help: "OpenSSL cipher list for TLS 1.2 and earlier; empty for the library's own.",
  },
];

export default function IcingaPage() {
  const panel = usePanel(ICINGA_MODULE, ICINGA_TABLE);
  const changes = panel.pending();
  const value = (key: string) => panel.value(ICINGA_PATH, key);
  const f = (s: FieldSpec) => (
    <KeyField
      key={s.key}
      panel={panel}
      path={ICINGA_PATH}
      k={s.key}
      label={s.label}
      kind={s.kind}
      help={s.help}
    />
  );
  const host = value("parent host");
  const off = offNote(host);
  const trust = firstUseRisk(host, value("trust on first use"), value("parent fingerprint"));
  const version = tlsVersionRisk(value("tls version"));
  const advancedSet = ADVANCED.filter(
    (a) => readStored(panel.stored, ICINGA_PATH, a.key) !== undefined,
  ).length;

  return (
    <PanelShell
      title="Icinga agent"
      subheader="Connects to an Icinga 2 master or satellite over the cluster protocol and runs the checks it schedules."
      panel={panel}
      changes={changes}
      problem={endpointProblem(host, value("parent endpoint"))}
      ini={icingaIni}
    >
      <PanelSection title="Parent">
        {off && <RiskBanner risk={off} />}
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          {f({
            key: "parent host",
            label: "Parent host",
            kind: "text",
            help: "Address of the Icinga 2 master or satellite.",
          })}
          <Stack sx={{ minWidth: 120 }}>
            {f({ key: "parent port", label: "Port", kind: "number" })}
          </Stack>
        </Stack>
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          {f({
            key: "parent endpoint",
            label: "Parent endpoint",
            kind: "text",
            help: "The CN of the parent's certificate; any other is refused.",
          })}
          {f({
            key: "endpoint name",
            label: "This agent's endpoint name",
            kind: "text",
            help: "Must match the Endpoint object on the master. Empty uses the lower-cased host name.",
          })}
        </Stack>
      </PanelSection>

      <PanelSection title="Trust">
        {trust && <RiskBanner risk={trust} />}
        {f({
          key: "parent fingerprint",
          label: "Parent fingerprint",
          kind: "text",
          help: "SHA-256 of the parent's certificate (openssl x509 -noout -fingerprint -sha256), to pin it before the agent has the Icinga CA.",
        })}
        {f({
          key: "trust on first use",
          label: "Trust the parent on first use",
          kind: "bool",
          help: "Allow the first connection without a fingerprint or CA, as icinga2 node setup does without --trustedcert.",
        })}
        {f({
          key: "ticket",
          label: "Ticket",
          kind: "secret",
          help: "From icinga2 pki ticket --cn <endpoint name>; the master signs the certificate at once. Treat it as a password.",
        })}
      </PanelSection>

      <PanelSection title="Checks">
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          {f({
            key: "threads",
            label: "Worker threads",
            kind: "number",
            help: "Checks run at the same time.",
          })}
        </Stack>
        {f({
          key: "allow arguments",
          label: "Allow arguments",
          kind: "bool",
          help: "Icinga CheckCommands carry their thresholds as arguments; off runs only argument-less queries and aliases. Which queries may run at all is the permission policy's job.",
        })}
        {version && <RiskBanner risk={version} />}
      </PanelSection>

      <AdvancedSection count={advancedSet}>{ADVANCED.map(f)}</AdvancedSection>
    </PanelShell>
  );
}
