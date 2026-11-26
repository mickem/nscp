// The TLS controls of a socket server: the mode, the certificate files for
// the TLS modes, and the TLS version.
import { Stack } from "@mui/material";
import { ReactNode } from "react";
import ModeSelector from "./ModeSelector.tsx";
import { Custom, ModeOption } from "./modes.ts";
import { KeyField } from "./PanelShell.tsx";
import RiskBanner, { Risk } from "./RiskBanner.tsx";
import type { SocketTlsDraft, SocketTlsView } from "./socketServer.ts";
import TlsSection from "./TlsSection.tsx";
import { SOCKET_TLS_VERSIONS, SocketTlsMode, tlsVersionRisk } from "./tls.ts";
import type { Panel } from "./usePanel.ts";
import { CUSTOM } from "./modes.ts";

interface Props {
  panel: Panel;
  path: string;
  view: SocketTlsView;
  setDraft: (update: (d: SocketTlsDraft) => SocketTlsDraft) => void;
  // What plain (no TLS) means for this protocol.
  plainDescription: ReactNode;
  // Said about the mode shown, if anything.
  risk: Risk | null;
  onExpertMode: () => void;
}

export default function SocketTlsControls({
  panel,
  path,
  view,
  setDraft,
  plainDescription,
  risk,
  onExpertMode,
}: Props) {
  const options: ModeOption<SocketTlsMode>[] = [
    {
      value: "two-way-tls",
      label: "Two-way TLS",
      description:
        "The agent presents a certificate and only accepts clients with a certificate issued by your CA.",
    },
    {
      value: "one-way-tls",
      label: "One-way TLS",
      description:
        "The agent presents a certificate and the traffic is encrypted, but clients are not authenticated by it.",
    },
    { value: "plain", label: "No TLS", description: plainDescription },
  ];
  const tls = view.mode === "two-way-tls" || view.mode === "one-way-tls";
  const versionRisk = tls ? tlsVersionRisk(panel.value(path, "tls version")) : null;
  return (
    <>
      {risk && <RiskBanner risk={risk} />}
      <ModeSelector
        label="How connections are secured"
        options={options}
        value={view.mode}
        storedIsCustom={view.current === CUSTOM}
        onChange={(mode: SocketTlsMode | Custom) => setDraft((d) => ({ ...d, mode }))}
        onExpertMode={onExpertMode}
        disabled={panel.busy}
      />
      {tls && (
        <TlsSection
          value={view.files}
          onChange={(files) => setDraft((d) => ({ ...d, files }))}
          showCa={view.mode === "two-way-tls"}
          disabled={panel.busy}
        />
      )}
      {tls && (
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          <KeyField
            panel={panel}
            path={path}
            k="tls version"
            label="TLS version"
            kind={{ options: SOCKET_TLS_VERSIONS }}
            help="A + allows that version and newer."
          />
        </Stack>
      )}
      {versionRisk && <RiskBanner risk={versionRisk} />}
    </>
  );
}
