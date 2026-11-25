// States plainly that a configuration is insecure, and why. Pages show it for
// what is stored (or about to be stored), not as a hint while typing.
import { Alert, AlertTitle } from "@mui/material";
import { ReactNode } from "react";

export interface Risk {
  // "error" for a configuration that exposes the host, "warning" for one that
  // is weaker than it should be.
  severity: "error" | "warning";
  title: string;
  reason: ReactNode;
}

export default function RiskBanner({ risk }: { risk: Risk }) {
  return (
    <Alert severity={risk.severity} role="alert">
      <AlertTitle>{risk.title}</AlertTitle>
      {risk.reason}
    </Alert>
  );
}
