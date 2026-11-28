// What the MSSQL page knows about CheckMSSQL's settings
// (modules/CheckMSSQL/CheckMSSQL.cpp, odbc_query.cpp).
import { fixed, KeyTable } from "../kit/keys.ts";
import type { Risk } from "../kit/RiskBanner.tsx";

export const MSSQL_MODULE = "CheckMSSQL";
export const MSSQL_PATH = "/settings/mssql";
export const MSSQL_FACTS = `${MSSQL_PATH}/facts`;

export const MSSQL_TABLE: KeyTable = {
  [MSSQL_PATH]: {
    hostname: { builtIn: fixed("localhost") },
    database: { builtIn: fixed("") },
    driver: { builtIn: fixed("") },
    user: { builtIn: fixed("") },
    password: { builtIn: fixed("") },
    "connection string": { builtIn: fixed("") },
    timeout: { builtIn: fixed("10") },
    "query timeout": { builtIn: fixed("30") },
  },
  [MSSQL_FACTS]: {
    mssql: { builtIn: fixed("false") },
    "mssql.databases": { builtIn: fixed("false") },
  },
};

// Tried in this order when `driver` is empty (odbc_query.cpp).
export const MSSQL_DRIVERS = [
  "ODBC Driver 18 for SQL Server",
  "ODBC Driver 17 for SQL Server",
  "ODBC Driver 13 for SQL Server",
  "ODBC Driver 11 for SQL Server",
  "SQL Server Native Client 11.0",
  "SQL Server",
];

export type MssqlLogin = "windows" | "sql";

export function loginOf(user: string, password: string): MssqlLogin {
  return user === "" && password === "" ? "windows" : "sql";
}

// What every check_mssql_* lets a caller do, whatever is configured: name
// another server (and driver, connection string, certificate trust) for the
// configured login to go to.
export const OVERRIDE_RISK: Risk = {
  severity: "warning",
  title: "Checks can be pointed at another server",
  reason:
    "Every check_mssql_* takes server=, connection-string= and the like from the caller, and the login configured here goes with them - a SQL password, or the agent's machine account under Windows authentication. Give the checks only to callers you trust, or a login that can do little.",
};

export function connectionStringRisk(cs: string): Risk | null {
  return /(^|;)\s*(pwd|password)\s*=/i.test(cs)
    ? {
        severity: "warning",
        title: "The connection string holds a password",
        reason:
          "It is stored as written and shown to anyone who can read the settings. Prefer Windows authentication, or the separate user and password, which the settings store as a secret.",
      }
    : null;
}
