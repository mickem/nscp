// What the MySQL page knows about CheckMySQL's settings
// (modules/CheckMySQL/CheckMySQL.cpp, mysql_session.cpp).
import { fixed, KeyTable } from "../kit/keys.ts";
import type { Risk } from "../kit/RiskBanner.tsx";

export const MYSQL_MODULE = "CheckMySQL";
export const MYSQL_PATH = "/settings/mysql";
export const MYSQL_FACTS = `${MYSQL_PATH}/facts`;

export const MYSQL_TABLE: KeyTable = {
  [MYSQL_PATH]: {
    hostname: { builtIn: fixed("localhost") },
    port: { builtIn: fixed("3306") },
    socket: { builtIn: fixed("") },
    database: { builtIn: fixed("") },
    user: { builtIn: fixed("") },
    password: { builtIn: fixed("") },
    "defaults file": { builtIn: fixed("") },
    tls: { builtIn: fixed("false") },
    "plugin dir": { builtIn: fixed("") },
    timeout: { builtIn: fixed("10") },
    "query timeout": { builtIn: fixed("30") },
  },
  [MYSQL_FACTS]: {
    mysql: { builtIn: fixed("false") },
    "mysql.databases": { builtIn: fixed("false") },
  },
};

const on = (v: string) => v.toLowerCase() === "true" || v === "1";

// Checks take host=, port=, user=, password= and tls= from the caller.
export const OVERRIDE_RISK: Risk = {
  severity: "warning",
  title: "Checks can be pointed at another server",
  reason:
    "Every check_mysql_* takes host=, port= and tls= from the caller, and the user and password configured here go with them. Use a login that can do little, and give the checks only to callers you trust.",
};

export function tlsRisk(tls: string, socket: string): Risk | null {
  if (socket !== "") return null;
  return on(tls)
    ? null
    : {
        severity: "warning",
        title: "TLS is not required",
        reason:
          "The connection uses TLS only if the client library and the server agree to by themselves, and the password may cross the network in the clear. Turn on Require TLS for a server that is not on this host.",
      };
}
