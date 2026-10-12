import { describe, expect, it } from "vitest";
import { screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import SettingsProblemsWidget from "./SettingsProblemsWidget";
import { SettingsProblem, SettingsProblems } from "../../api/api";
import { installFetchMock, jsonResponse, renderWithProviders } from "../../test/test-utils";

const problem = (over: Partial<SettingsProblem> = {}): SettingsProblem => ({
  kind: "relative_path",
  severity: "warning",
  source: "/etc/nsclient/nsclient.ini",
  path: "settings/external scripts/scripts",
  key: "",
  suggestion: "/settings/external scripts/scripts",
  message:
    "Section [settings/external scripts/scripts] is ignored: settings paths start with '/'. Did you mean [/settings/external scripts/scripts]?",
  ...over,
});

const report = (problems: SettingsProblem[]): SettingsProblems => ({
  problems,
  count: problems.length,
  warnings: problems.filter((p) => p.severity === "warning").length,
});

const unusedSection = problem({
  kind: "unused_path",
  severity: "info",
  path: "/settings/NRPE/server",
  suggestion: "",
  message: "Section [/settings/NRPE/server] is not used by any loaded module (it may belong to a module that is not enabled).",
});

describe("SettingsProblemsWidget", () => {
  it("raises a banner for entries nothing reads", async () => {
    installFetchMock({ "/api/v2/settings/problems": jsonResponse(report([problem(), unusedSection])) });
    renderWithProviders(<SettingsProblemsWidget />, { withRouter: false });

    await waitFor(() =>
      expect(screen.getByText(/1 entry in the configuration is ignored because nothing reads it/)).toBeInTheDocument(),
    );
  });

  // A disabled module's leftover section is how most configurations look;
  // nagging about it on every page would teach operators to ignore the banner.
  it("stays quiet when there is only info", async () => {
    const fetchMock = installFetchMock({ "/api/v2/settings/problems": jsonResponse(report([unusedSection])) });
    renderWithProviders(<SettingsProblemsWidget />, { withRouter: false });

    await waitFor(() => expect(fetchMock).toHaveBeenCalled());
    expect(screen.queryByRole("alert")).not.toBeInTheDocument();
  });

  // A role without settings.get gets an empty report rather than a 403, which
  // the UI would take for an expired session and log the user out.
  it("stays quiet for a role that may not read settings", async () => {
    const fetchMock = installFetchMock({
      "/api/v2/settings/problems": jsonResponse({ ...report([]), restricted: true }),
    });
    renderWithProviders(<SettingsProblemsWidget />, { withRouter: false });

    await waitFor(() => expect(fetchMock).toHaveBeenCalled());
    expect(screen.queryByRole("alert")).not.toBeInTheDocument();
  });

  // The UI bundle can be newer than the agent serving it (`nscp web
  // install-ui`), and an older agent has no such endpoint.
  it("stays quiet when the agent does not have the endpoint", async () => {
    const fetchMock = installFetchMock({ "/api/v2/settings/problems": new Response("Not found", { status: 404 }) });
    renderWithProviders(<SettingsProblemsWidget />, { withRouter: false });

    await waitFor(() => expect(fetchMock).toHaveBeenCalled());
    expect(screen.queryByRole("alert")).not.toBeInTheDocument();
  });

  it("lists the warnings, and the info entries on request", async () => {
    installFetchMock({ "/api/v2/settings/problems": jsonResponse(report([problem(), unusedSection])) });
    renderWithProviders(<SettingsProblemsWidget />, { withRouter: false });

    await userEvent.click(await screen.findByRole("button", { name: "Show details" }));

    expect(await screen.findByText("[settings/external scripts/scripts]")).toBeInTheDocument();
    expect(screen.getByText(/Did you mean \[\/settings\/external scripts\/scripts\]/)).toBeInTheDocument();
    expect(screen.queryByText("[/settings/NRPE/server]")).not.toBeInTheDocument();

    await userEvent.click(screen.getByLabelText("Show info (1)"));
    expect(screen.getByText("[/settings/NRPE/server]")).toBeInTheDocument();
  });

  it("names the key and the section it is in", async () => {
    installFetchMock({
      "/api/v2/settings/problems": jsonResponse(
        report([
          problem({
            kind: "unknown_key",
            path: "/settings/external scripts",
            key: "alow arguments",
            suggestion: "allow arguments",
            message: "Key 'alow arguments' in [/settings/external scripts] is not a known key. Did you mean 'allow arguments'?",
          }),
        ]),
      ),
    });
    renderWithProviders(<SettingsProblemsWidget />, { withRouter: false });

    await userEvent.click(await screen.findByRole("button", { name: "Show details" }));
    expect(await screen.findByText("[/settings/external scripts] alow arguments")).toBeInTheDocument();
  });
});
