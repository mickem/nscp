// The credentialed check pages against a fake settings API.
import { describe, expect, it } from "vitest";
import { ComponentType } from "react";
import { screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { Settings } from "../api/api";
import {
  authenticatedState,
  installFetchMock,
  jsonResponse,
  renderWithProviders,
} from "../test/test-utils";
import MssqlPage from "./CheckMSSQL/MssqlPage";
import { connectionStringRisk } from "./CheckMSSQL/mssql";
import MysqlPage from "./CheckMySQL/MysqlPage";
import WmiPage from "./CheckWMI/WmiPage";
import WmiQueries from "./CheckWMI/WmiQueries";
import { accessRisk, queryAccessOf } from "./CheckWMI/wmi";
import { endpointProblem } from "./CheckDocker/docker";
import KubernetesPage from "./CheckKubernetes/KubernetesPage";
import { kubeProblem, kubeRisks, sourceOf } from "./CheckKubernetes/kubernetes";

function render(Page: ComponentType, stored: Settings[]) {
  const writes: string[] = [];
  installFetchMock({
    "/api/v2/settings": async (req: Request) => {
      if (req.method === "PUT") {
        const body = (await req.json()) as Settings;
        writes.push(`[${body.path}] ${body.key}=${body.value}`);
        return jsonResponse("");
      }
      return jsonResponse(stored);
    },
  });
  renderWithProviders(<Page />, { preloadedState: authenticatedState });
  return writes;
}
const save = () => userEvent.click(screen.getByRole("button", { name: "Save" }));

describe("MSSQL", () => {
  const P = "/settings/mssql";
  it("reads no login as Windows authentication, and says checks can be pointed elsewhere", async () => {
    render(MssqlPage, []);
    expect(await screen.findByRole("radio", { name: /Windows authentication/ })).toBeChecked();
    expect(screen.getByText("Checks can be pointed at another server")).toBeInTheDocument();
    expect(screen.queryByLabelText("User")).not.toBeInTheDocument();
  });

  it("writes a SQL login", async () => {
    const writes = render(MssqlPage, []);
    await userEvent.click(await screen.findByRole("radio", { name: "A SQL Server login" }));
    await userEvent.type(screen.getByLabelText("User"), "monitor");
    await userEvent.type(screen.getByLabelText("Password"), "pw");
    await save();
    await waitFor(() =>
      expect(writes).toEqual([
        `[${P}] user=monitor`,
        `[${P}] password=pw`,
        "[/modules] CheckMSSQL=enabled",
      ]),
    );
  });

  it("keeps browsers and password managers off the login fields", async () => {
    render(MssqlPage, []);
    await userEvent.click(await screen.findByRole("radio", { name: "A SQL Server login" }));
    const user = screen.getByLabelText("User");
    const password = screen.getByLabelText("Password");
    expect(user).toHaveAttribute("autocomplete", "off");
    expect(password).toHaveAttribute("autocomplete", "new-password");
    for (const input of [user, password]) {
      expect(input).toHaveAttribute("data-1p-ignore", "true");
      expect(input).toHaveAttribute("data-lpignore", "true");
      expect(input).toHaveAttribute("data-bwignore", "true");
      expect(input).toHaveAttribute("data-form-type", "other");
    }
  });

  it("reads a connection string as the whole connection, and flags a password in it", async () => {
    render(MssqlPage, [{ path: P, key: "connection string", value: "Driver={x};Server=db;PWD=s" }]);
    expect(await screen.findByRole("radio", { name: "ODBC connection string" })).toBeChecked();
    expect(screen.queryByLabelText("Server")).not.toBeInTheDocument();
    expect(screen.queryByText("Login")).not.toBeInTheDocument();
    expect(screen.getByText("The connection string holds a password")).toBeInTheDocument();
    expect(connectionStringRisk("Server=db;Trusted_Connection=yes")).toBeNull();
  });
});

describe("MySQL", () => {
  const P = "/settings/mysql";
  it("says TLS is not required, and a socket needs no TLS", async () => {
    render(MysqlPage, []);
    expect(await screen.findByText("TLS is not required")).toBeInTheDocument();
    await userEvent.click(
      screen.getByRole("radio", { name: "Through a local socket or named pipe" }),
    );
    expect(screen.queryByText("TLS is not required")).not.toBeInTheDocument();
    expect(screen.queryByLabelText("Port")).not.toBeInTheDocument();
  });

  it("takes the login from an option file, dropping the one set here", async () => {
    const writes = render(MysqlPage, [
      { path: P, key: "user", value: "u" },
      { path: P, key: "password", value: "p" },
    ]);
    await userEvent.click(await screen.findByRole("radio", { name: /From an option file/ }));
    await userEvent.type(screen.getByLabelText("Option file"), "/etc/nscp/my.cnf");
    await save();
    await waitFor(() =>
      expect(writes).toEqual([
        `[${P}] user=`,
        `[${P}] password=`,
        `[${P}] defaults file=/etc/nscp/my.cnf`,
        "[/modules] CheckMySQL=enabled",
      ]),
    );
  });
});

describe("WMI", () => {
  it("reads the agent's spellings of query access, failing closed on others", () => {
    expect(queryAccessOf("")).toBe("any");
    expect(queryAccessOf(" List ")).toBe("allowed");
    expect(queryAccessOf("named")).toBe("predefined");
    expect(queryAccessOf("some")).toBeNull();
    expect(accessRisk(null, "")?.title).toMatch(/not one the agent reads/);
    expect(accessRisk("allowed", "")?.title).toBe("No class is allowed");
    expect(accessRisk("predefined", "")).toBeNull();
  });

  it("restricts queries to allowed classes", async () => {
    const writes = render(WmiPage, []);
    expect(await screen.findByText("Callers can run any WMI query")).toBeInTheDocument();
    await userEvent.click(screen.getByRole("radio", { name: /Plain SELECTs/ }));
    await userEvent.type(screen.getByLabelText("Allowed classes"), "Win32_Service");
    await save();
    await waitFor(() =>
      expect(writes).toEqual([
        "[/settings/wmi] query access=allowed",
        "[/settings/wmi] allowed classes=Win32_Service",
        "[/modules] CheckWMI=enabled",
      ]),
    );
  });
});

describe("WMI named queries", () => {
  it("runs a saved query by name, and a typed one as written", async () => {
    const runs: string[] = [];
    installFetchMock({
      "/api/v2/settings": jsonResponse([
        { path: "/settings/wmi/queries", key: "services", value: "SELECT Name FROM Win32_Service" },
      ]),
      "/api/v2/queries/check_wmi/commands/execute": async (req: Request) => {
        runs.push(new URL(req.url, "http://localhost").searchParams.get("query") ?? "");
        return jsonResponse({
          command: "check_wmi",
          result: 0,
          lines: [{ message: "OK: 142 rows", perf: {} }],
        });
      },
    });
    renderWithProviders(<WmiQueries />, { preloadedState: authenticatedState });
    await userEvent.click(await screen.findByRole("button", { name: "Run" }));
    expect(await screen.findByText(/OK: OK: 142 rows/)).toBeInTheDocument();
    await userEvent.keyboard("{Escape}");
    await userEvent.click(screen.getByRole("button", { name: "Add query" }));
    await userEvent.type(screen.getByLabelText("WQL"), "SELECT Name FROM Win32_Process");
    await userEvent.click(screen.getByRole("button", { name: "Run now" }));
    await waitFor(() => expect(runs).toEqual(["services", "SELECT Name FROM Win32_Process"]));
  });
});

describe("Docker", () => {
  it("accepts only a local socket or named pipe", () => {
    expect(endpointProblem("")).toBeNull();
    expect(endpointProblem("/var/run/docker.sock")).toBeNull();
    expect(endpointProblem("\\\\.\\pipe\\docker_engine")).toBeNull();
    expect(endpointProblem("tcp://host:2375")).toMatch(/local socket/);
    expect(endpointProblem("/var/run/../docker.sock")).toMatch(/local socket/);
  });
});

describe("Kubernetes", () => {
  const get = (m: Record<string, string>) => (k: string) =>
    m[k] ??
    (
      {
        "verify mode": "peer",
        "tls version": "tlsv1.2+",
        timeout: "30",
        "max response size": "64",
      } as Record<string, string>
    )[k] ??
    "";

  it("finds the cluster as the agent does: API server, then kubeconfig, then in-cluster", () => {
    expect(sourceOf(get({ "api server": "https://k", kubeconfig: "/k.json" }))).toBe("server");
    expect(sourceOf(get({ kubeconfig: "/k.json" }))).toBe("kubeconfig");
    expect(sourceOf(get({}))).toBe("incluster");
    expect(kubeProblem(get({ "api server": "https://k" }), "server")).toMatch(/token/);
  });

  it("flags a token in clear, an unverified server and a stored token", () => {
    const titles = kubeRisks(
      get({ "api server": "http://k", token: "t", "verify mode": "none" }),
      "server",
    ).map((r) => r.title);
    expect(titles).toEqual([
      "The token is sent in clear",
      "The API server is not verified",
      "The token is stored in the settings",
    ]);
  });

  it("needs a token once an API server is chosen", async () => {
    render(KubernetesPage, []);
    await userEvent.click(
      await screen.findByRole("radio", { name: "An API server and a bearer token" }),
    );
    await userEvent.type(screen.getByLabelText("API server"), "https://k:6443");
    expect(
      screen.getByText("With an API server set, the agent needs a token or a token file."),
    ).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Save" })).toBeDisabled();
    await userEvent.type(screen.getByLabelText("Token file"), "/etc/nscp/kube.token");
    expect(screen.getByRole("button", { name: "Save" })).toBeEnabled();
  });
});
