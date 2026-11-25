// Explains a setting whose real control lives elsewhere, with a link there.
import { Alert, Link } from "@mui/material";
import { Link as RouterLink } from "react-router";
import { ReactNode } from "react";

interface Props {
  children: ReactNode;
  // Where the setting is really configured, e.g. "/modules/CheckExternalScripts".
  to?: string;
  linkText?: string;
}

export default function Explainer({ children, to, linkText }: Props) {
  return (
    <Alert severity="info" variant="outlined">
      {children}
      {to && (
        <>
          {" "}
          <Link component={RouterLink} to={to}>
            {linkText ?? "Open"}
          </Link>
        </>
      )}
    </Alert>
  );
}
