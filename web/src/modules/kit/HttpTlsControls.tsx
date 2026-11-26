// The TLS fields of an HTTP listener.
import { Stack } from "@mui/material";
import { insecureRisk } from "./httpTls.ts";
import { KeyField } from "./PanelShell.tsx";
import RiskBanner from "./RiskBanner.tsx";
import { HTTP_TLS_VERSIONS, tlsVersionRisk } from "./tls.ts";
import type { Panel } from "./usePanel.ts";

interface Props {
  panel: Panel;
  path: string;
  // What a clear connection would give away.
  exposes: string;
}

export default function HttpTlsControls({ panel, path, exposes }: Props) {
  const risk = insecureRisk(panel.value(path, "allow insecure"), exposes);
  const versionRisk = tlsVersionRisk(panel.value(path, "tls version"));
  return (
    <>
      {risk && <RiskBanner risk={risk} />}
      <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
        <KeyField
          panel={panel}
          path={path}
          k="certificate"
          label="Certificate"
          kind="text"
          help="A default one is generated when the default file is missing."
        />
        <KeyField
          panel={panel}
          path={path}
          k="certificate key"
          label="Certificate key"
          kind="text"
          help="Leave empty if the key is in the certificate file."
        />
      </Stack>
      <KeyField
        panel={panel}
        path={path}
        k="tls version"
        label="TLS version"
        kind={{ options: HTTP_TLS_VERSIONS }}
        help="A + allows that version and newer; any allows all."
      />
      {versionRisk && <RiskBanner risk={versionRisk} />}
      <KeyField
        panel={panel}
        path={path}
        k="allow insecure"
        label="Serve plain HTTP when the certificate is missing"
        kind="bool"
        help="Off refuses to start instead. A certificate that is there but does not load always stops the listener."
      />
    </>
  );
}
