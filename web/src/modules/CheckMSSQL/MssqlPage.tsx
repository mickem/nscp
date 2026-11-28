// The MSSQL page: which server, how the agent logs in, and the fact sets.
import { FormControlLabel, Radio, RadioGroup, Stack } from "@mui/material";
import { useState } from "react";
import { readStored } from "../kit/changes.ts";
import { tableIni } from "../kit/keys.ts";
import PanelShell, { AdvancedSection, KeyField, PanelSection } from "../kit/PanelShell.tsx";
import RiskBanner from "../kit/RiskBanner.tsx";
import type { FieldSpec } from "../kit/socketServer.ts";
import { usePanel } from "../kit/usePanel.ts";
import {
  connectionStringRisk,
  loginOf,
  MSSQL_DRIVERS,
  MSSQL_FACTS,
  MSSQL_MODULE,
  MSSQL_PATH,
  MSSQL_TABLE,
  MssqlLogin,
  OVERRIDE_RISK,
} from "./mssql.ts";

type Connection = "fields" | "string";

export default function MssqlPage() {
  const panel = usePanel(MSSQL_MODULE, MSSQL_TABLE);
  const changes = panel.pending();
  const value = (key: string) => panel.value(MSSQL_PATH, key);
  const f = (s: FieldSpec, path = MSSQL_PATH) => (
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
  const [connection, setConnection] = useState<Connection | null>(null);
  const [login, setLogin] = useState<MssqlLogin | null>(null);
  const shownConnection = connection ?? (value("connection string") ? "string" : "fields");
  const shownLogin = login ?? loginOf(value("user"), value("password"));
  const csRisk = connectionStringRisk(value("connection string"));
  const advancedSet = ["timeout", "query timeout"].filter(
    (k) => readStored(panel.stored, MSSQL_PATH, k) !== undefined,
  ).length;

  return (
    <PanelShell
      title="Microsoft SQL Server"
      subheader="Checks a SQL Server's health, databases and queries over ODBC (Windows)."
      panel={panel}
      changes={changes}
      problem={
        shownConnection === "string" && !value("connection string").trim()
          ? "Give the connection string, or build the connection from the fields."
          : null
      }
      test={{
        query: "check_mssql",
        what: "Connects with the saved settings and reads the server version: proves the server, driver and login.",
      }}
      ini={(stored, d) => tableIni(stored, MSSQL_MODULE, MSSQL_TABLE, d)}
    >
      <PanelSection title="Server">
        <RadioGroup
          row
          value={shownConnection}
          onChange={(e) => {
            const next = e.target.value as Connection;
            setConnection(next);
            // The connection string replaces every other field: leaving one
            // behind would keep it in charge.
            if (next === "fields") panel.set(MSSQL_PATH, "connection string", "");
          }}
        >
          <FormControlLabel
            value="fields"
            control={<Radio size="small" />}
            label="Server and database"
          />
          <FormControlLabel
            value="string"
            control={<Radio size="small" />}
            label="ODBC connection string"
          />
        </RadioGroup>
        {shownConnection === "fields" ? (
          <>
            <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
              {f({
                key: "hostname",
                label: "Server",
                kind: "text",
                help: "host, host\\INSTANCE or host,port.",
              })}
              {f({
                key: "database",
                label: "Database",
                kind: "text",
                help: "The login's default database when empty.",
              })}
            </Stack>
            {f({
              key: "driver",
              label: "ODBC driver",
              kind: { options: ["", ...MSSQL_DRIVERS] },
              help: "Empty picks the newest installed: ODBC Driver 18, 17, 13, 11, Native Client 11.0, then SQL Server. Driver 18 encrypts the connection, 17 and older do not; either way the server's certificate is not checked (TrustServerCertificate).",
            })}
          </>
        ) : (
          <>
            {csRisk && <RiskBanner risk={csRisk} />}
            {f({
              key: "connection string",
              label: "Connection string",
              kind: "text",
              help: "Used as written: server, database, driver and login below are ignored. Driver={ODBC Driver 18 for SQL Server};Server=db1;Trusted_Connection=yes;",
            })}
          </>
        )}
      </PanelSection>

      {shownConnection === "fields" && (
        <PanelSection title="Login">
          <RiskBanner risk={OVERRIDE_RISK} />
          <RadioGroup
            value={shownLogin}
            onChange={(e) => {
              const next = e.target.value as MssqlLogin;
              setLogin(next);
              if (next === "windows") {
                panel.set(MSSQL_PATH, "user", "");
                panel.set(MSSQL_PATH, "password", "");
              }
            }}
          >
            <FormControlLabel
              value="windows"
              control={<Radio size="small" />}
              label="Windows authentication, as the agent's service account (no password stored)"
            />
            <FormControlLabel
              value="sql"
              control={<Radio size="small" />}
              label="A SQL Server login"
            />
          </RadioGroup>
          {shownLogin === "sql" && (
            <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
              {f({ key: "user", label: "User", kind: "text" })}
              {f({ key: "password", label: "Password", kind: "secret" })}
            </Stack>
          )}
        </PanelSection>
      )}

      <PanelSection title="Facts">
        {f(
          {
            key: "mssql",
            label: "Report the server",
            kind: "bool",
            help: "Version, edition and the like, as the mssql fact set; collected from the first round after start.",
          },
          MSSQL_FACTS,
        )}
        {f(
          {
            key: "mssql.databases",
            label: "Report its databases",
            kind: "bool",
            help: "Every database the login can see, as mssql.databases.",
          },
          MSSQL_FACTS,
        )}
      </PanelSection>

      <AdvancedSection count={advancedSet}>
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          {f({ key: "timeout", label: "Login timeout (seconds)", kind: "number" })}
          {f({ key: "query timeout", label: "Query timeout (seconds)", kind: "number" })}
        </Stack>
      </AdvancedSection>
    </PanelShell>
  );
}
