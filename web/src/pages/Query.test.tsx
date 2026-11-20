import { describe, expect, it } from "vitest";
import { screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { Route, Routes } from "react-router";
import Query from "./Query";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

const COMMON = "\nCommon option for all filter checks.";

const option = (name: string, default_value = "") => ({
  name,
  default_value,
  required: false,
  repeatable: false,
  content_type: "string",
  short_description: name,
  long_description: name + COMMON,
});

const field = (name: string) => ({ name, short_description: name, long_description: name });

const queryInfo = (name: string) => ({
  name,
  title: name,
  plugin: "CheckSystem",
  description: "Description of " + name,
  experimental: false,
  execute_nagios_url: "",
  execute_url: "",
});

const helpFor = (name: string, warning: string, critical: string, fields: string[]) => ({
  name,
  keyword_source: name,
  parameters: [option("filter"), option("warning", warning), option("warn"), option("critical", critical), option("crit")],
  fields: fields.map(field),
});

function setup(name: string, warning: string, critical: string, fields: string[] = ["load", "core"]) {
  const executed: string[] = [];
  installFetchMock({
    [`/api/v2/queries/${name}`]: jsonResponse(queryInfo(name)),
    [`/api/v2/queries/${name}/help`]: jsonResponse(helpFor(name, warning, critical, fields)),
    [`/api/v2/queries/${name}/commands/execute`]: (req) => {
      executed.push(new URL(req.url, "http://localhost").search);
      return jsonResponse({ command: name, result: 0, lines: [{ message: "OK: CPU load is ok.", perf: {} }] });
    },
  });
  const view = renderWithProviders(
    <Routes>
      <Route path="queries/:id" element={<Query />} />
    </Routes>,
    { route: `/queries/${name}` },
  );
  return { ...view, executed };
}

const argumentLine = () => screen.getByRole("textbox", { name: "Arguments" }) as HTMLTextAreaElement;
const simpleSwitch = () => screen.getByLabelText("Simple mode");

describe("Query page simple mode", () => {
  it("starts with an empty argument line and the switch off", async () => {
    setup("check_cpu", "load > 80", "load > 90");
    await screen.findByText("Description of check_cpu");

    expect(simpleSwitch()).not.toBeChecked();
    expect(argumentLine().value).toBe("");
  });

  it("writes the check's default thresholds onto the line when switched on", async () => {
    setup("check_cpu", "load > 80", "load > 90");
    await screen.findByText("Description of check_cpu");
    await waitFor(() => expect(screen.queryByRole("progressbar")).not.toBeInTheDocument());

    await userEvent.click(simpleSwitch());

    await waitFor(() => expect(argumentLine().value).toBe('"warning=load > 80" "critical=load > 90"'));
    expect(screen.getByText(/Sample thresholds added: the values this check uses by default/)).toBeInTheDocument();
  });

  it("sends the sample as one argument per threshold", async () => {
    const { executed } = setup("check_cpu", "load > 80", "load > 90");
    await screen.findByText("Description of check_cpu");
    await userEvent.click(simpleSwitch());
    await waitFor(() => expect(argumentLine().value).not.toBe(""));

    await userEvent.click(screen.getByRole("button", { name: "Execute" }));

    await waitFor(() => expect(executed).toHaveLength(1));
    const sent = new URLSearchParams(executed[0]);
    expect(sent.get("warning")).toBe("load > 80");
    expect(sent.get("critical")).toBe("load > 90");
  });

  it("takes the sample out again when switched off", async () => {
    setup("check_cpu", "load > 80", "load > 90");
    await screen.findByText("Description of check_cpu");
    await userEvent.type(argumentLine(), "time=5m");
    await userEvent.click(simpleSwitch());
    await waitFor(() => expect(argumentLine().value).toBe('time=5m "warning=load > 80" "critical=load > 90"'));

    await userEvent.click(simpleSwitch());

    expect(argumentLine().value).toBe("time=5m");
    expect(screen.queryByText(/Sample thresholds added/)).not.toBeInTheDocument();
  });

  it("keeps a threshold the user typed and only adds the missing level", async () => {
    setup("check_cpu", "load > 80", "load > 90");
    await screen.findByText("Description of check_cpu");
    await userEvent.type(argumentLine(), "warn=none");

    await userEvent.click(simpleSwitch());

    await waitFor(() => expect(argumentLine().value).toBe('warn=none "critical=load > 90"'));
  });

  it("says so when the check has no thresholds to sample", async () => {
    setup("check_os_version", "", "", ["os", "kernel_release"]);
    await screen.findByText("Description of check_os_version");

    await userEvent.click(simpleSwitch());

    expect(await screen.findByText(/no default thresholds, so there is no sample to add/)).toBeInTheDocument();
    expect(argumentLine().value).toBe("");
  });

  it("remembers the mode, so the next check opens with its sample in place", async () => {
    const first = setup("check_cpu", "load > 80", "load > 90");
    await screen.findByText("Description of check_cpu");
    await userEvent.click(simpleSwitch());
    await waitFor(() => expect(argumentLine().value).not.toBe(""));
    first.unmount();

    setup("check_memory", "used > 80%", "used > 90%", ["used", "free"]);
    await screen.findByText("Description of check_memory");

    expect(simpleSwitch()).toBeChecked();
    await waitFor(() => expect(argumentLine().value).toBe('"warning=used > 80%" "critical=used > 90%"'));
  });
});
