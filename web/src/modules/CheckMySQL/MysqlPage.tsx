// The MySQL page: which server and how, how the agent logs in, and the
// fact sets.
import { FormControlLabel, Radio, RadioGroup, Stack } from "@mui/material";
import { useState } from "react";
import { readStored } from "../kit/changes.ts";
import { tableIni } from "../kit/keys.ts";
import PanelShell, { AdvancedSection, KeyField, PanelSection } from "../kit/PanelShell.tsx";
import RiskBanner from "../kit/RiskBanner.tsx";
import type { FieldSpec } from "../kit/socketServer.ts";
import { usePanel } from "../kit/usePanel.ts";
import {
  MYSQL_FACTS,
  MYSQL_MODULE,
  MYSQL_PATH,
  MYSQL_TABLE,
  OVERRIDE_RISK,
  tlsRisk,
} from "./mysql.ts";

type Via = "tcp" | "socket";
type Login = "here" | "file";

export default function MysqlPage() {
  const panel = usePanel(MYSQL_MODULE, MYSQL_TABLE);
  const changes = panel.pending();
  const value = (key: string) => panel.value(MYSQL_PATH, key);
  const f = (s: FieldSpec, path = MYSQL_PATH) => (
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
  const [via, setVia] = useState<Via | null>(null);
  const [login, setLogin] = useState<Login | null>(null);
  const shownVia = via ?? (value("socket") ? "socket" : "tcp");
  const shownLogin = login ?? (value("defaults file") && !value("user") ? "file" : "here");
  const tRisk = tlsRisk(value("tls"), shownVia === "socket" ? "x" : "");
  const advancedSet = ["plugin dir", "timeout", "query timeout"].filter(
    (k) => readStored(panel.stored, MYSQL_PATH, k) !== undefined,
  ).length;

  return (
    <PanelShell
      title="MySQL and MariaDB"
      subheader="Checks a MySQL or MariaDB server's health, databases and queries."
      panel={panel}
      changes={changes}
      problem={null}
      test={{
        query: "check_mysql",
        what: "Connects with the saved settings and reads the server version: proves the server, TLS and login.",
      }}
      ini={(stored, d) => tableIni(stored, MYSQL_MODULE, MYSQL_TABLE, d)}
    >
      <PanelSection title="Server">
        <RadioGroup
          row
          value={shownVia}
          onChange={(e) => {
            const next = e.target.value as Via;
            setVia(next);
            // A socket wins over host and port, so it has to go for TCP.
            if (next === "tcp") panel.set(MYSQL_PATH, "socket", "");
          }}
        >
          <FormControlLabel
            value="tcp"
            control={<Radio size="small" />}
            label="Over the network (TCP)"
          />
          <FormControlLabel
            value="socket"
            control={<Radio size="small" />}
            label="Through a local socket or named pipe"
          />
        </RadioGroup>
        {shownVia === "tcp" ? (
          <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
            {f({
              key: "hostname",
              label: "Server",
              kind: "text",
              help: "localhost is reached over TCP too.",
            })}
            <Stack sx={{ minWidth: 120 }}>
              {f({ key: "port", label: "Port", kind: "number" })}
            </Stack>
          </Stack>
        ) : (
          f({
            key: "socket",
            label: "Socket",
            kind: "text",
            help: "/var/run/mysqld/mysqld.sock, or a named pipe on Windows. Server and port are not used.",
          })
        )}
        {f({
          key: "database",
          label: "Database",
          kind: "text",
          help: "The one to connect to; none when empty.",
        })}
        {tRisk && <RiskBanner risk={tRisk} />}
        {shownVia === "tcp" &&
          f({
            key: "tls",
            label: "Require TLS",
            kind: "bool",
            help: "Refuse a connection without TLS. The agent does not check the server's certificate itself; the client library may.",
          })}
      </PanelSection>

      <PanelSection title="Login">
        <RiskBanner risk={OVERRIDE_RISK} />
        <RadioGroup
          value={shownLogin}
          onChange={(e) => {
            const next = e.target.value as Login;
            setLogin(next);
            if (next === "file") {
              panel.set(MYSQL_PATH, "user", "");
              panel.set(MYSQL_PATH, "password", "");
            }
          }}
        >
          <FormControlLabel
            value="here"
            control={<Radio size="small" />}
            label="A user and password set here"
          />
          <FormControlLabel
            value="file"
            control={<Radio size="small" />}
            label="From an option file (my.cnf), kept out of the agent's settings"
          />
        </RadioGroup>
        {shownLogin === "here" ? (
          <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
            {f({ key: "user", label: "User", kind: "text" })}
            {f({ key: "password", label: "Password", kind: "secret" })}
          </Stack>
        ) : (
          f({
            key: "defaults file",
            label: "Option file",
            kind: "text",
            help: "Its [client] group supplies user, password and database; the server and port set here still win.",
          })
        )}
      </PanelSection>

      <PanelSection title="Facts">
        {f(
          {
            key: "mysql",
            label: "Report the server",
            kind: "bool",
            help: "Version and the like, as the mysql fact set; collected from the first round after start.",
          },
          MYSQL_FACTS,
        )}
        {f(
          {
            key: "mysql.databases",
            label: "Report its databases",
            kind: "bool",
            help: "Every database the login can see, as mysql.databases.",
          },
          MYSQL_FACTS,
        )}
      </PanelSection>

      <AdvancedSection count={advancedSet}>
        <Stack direction={{ xs: "column", md: "row" }} spacing={2}>
          {f({ key: "timeout", label: "Connect timeout (seconds)", kind: "number" })}
          {f({ key: "query timeout", label: "Query timeout (seconds)", kind: "number" })}
        </Stack>
        {f({
          key: "plugin dir",
          label: "Client plugin folder",
          kind: "text",
          help: "Where the client loads auth plugins such as caching_sha2_password; the library's own when empty.",
        })}
      </AdvancedSection>
    </PanelShell>
  );
}
