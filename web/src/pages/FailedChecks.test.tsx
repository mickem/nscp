import { describe, expect, it } from "vitest";
import { screen, waitFor, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import FailedChecks from "./FailedChecks";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

const ENTRIES = [
  {
    index: 2,
    timestamp: 1760000060,
    date: "2025-Oct-09 10:01:00",
    age: 30,
    subject: "WEBServer:admin",
    depth: 0,
    command: "check_drivesize",
    alias: "",
    status: 2,
    result: "CRITICAL",
    message: "CRITICAL C: 92% used",
    lines: [
      {
        message: "CRITICAL C: 92% used",
        perf: { "C: used %": { value: 92, unit: "%", warning: 80, critical: "@90:100", minimum: 0, maximum: 100 } },
      },
    ],
    arguments: ["drive=C:", "password=***"],
  },
  {
    index: 1,
    timestamp: 1760000000,
    date: "2025-Oct-09 10:00:00",
    age: 90,
    subject: "NRPEServer",
    depth: 1,
    command: "check_cpu",
    alias: "cpu",
    status: 1,
    result: "WARNING",
    message: "WARNING: 5m: 85%",
    lines: [{ message: "WARNING: 5m: 85%", perf: {} }],
    arguments: [],
  },
];

const envelope = (over: Record<string, unknown> = {}) =>
  jsonResponse({
    enabled: true,
    max_entries: 50,
    arguments_recorded: true,
    statuses: "warning,critical,unknown",
    last_index: 2,
    dropped: 3,
    count: ENTRIES.length,
    entries: ENTRIES,
    ...over,
  });

describe("FailedChecks", () => {
  it("lists the log newest first with status, command, message and source", async () => {
    installFetchMock({ "/api/v2/failures": envelope() });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    const rows = screen.getAllByRole("row");
    // Header, then the two entries (each followed by its collapsed detail row).
    expect(within(rows[1]).getByText("Critical")).toBeInTheDocument();
    expect(within(rows[1]).getByText("CRITICAL C: 92% used")).toBeInTheDocument();
    expect(within(rows[1]).getByText("WEBServer:admin")).toBeInTheDocument();
    expect(screen.getByText("check_cpu")).toBeInTheDocument();
    expect(screen.getByText("Warning")).toBeInTheDocument();
    expect(screen.getByText("NRPEServer")).toBeInTheDocument();
    // A check run by another check is marked as such.
    expect(screen.getByText("nested")).toBeInTheDocument();
    // The configuration line: cap, evictions and whether arguments are kept.
    expect(screen.getByText(/Keeping the last 50 checks/)).toBeInTheDocument();
    expect(screen.getByText(/3 older entries evicted/)).toBeInTheDocument();
    expect(screen.getByText(/· arguments recorded/)).toBeInTheDocument();
  });

  it("expands an entry to its lines, perf data and arguments", async () => {
    installFetchMock({ "/api/v2/failures": envelope() });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    await userEvent.click(screen.getAllByRole("button", { name: "expand" })[0]);

    const perf = screen.getByRole("table", { name: "performance data" });
    expect(within(perf).getByText("C: used %")).toBeInTheDocument();
    expect(within(perf).getByText("92")).toBeInTheDocument();
    // A Nagios range threshold is shown as the string it was.
    expect(within(perf).getByText("@90:100")).toBeInTheDocument();
    expect(screen.getByText("drive=C:")).toBeInTheDocument();
    // Redacted by the agent; the UI shows what it got.
    expect(screen.getByText("password=***")).toBeInTheDocument();
  });

  it("says when arguments were not recorded rather than showing an empty list", async () => {
    const stripped = ENTRIES.map((e) => {
      const copy: Record<string, unknown> = { ...e };
      delete copy.arguments;
      return copy;
    });
    installFetchMock({ "/api/v2/failures": envelope({ arguments_recorded: false, entries: stripped }) });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    expect(screen.getByText(/arguments not recorded/)).toBeInTheDocument();
    await userEvent.click(screen.getAllByRole("button", { name: "expand" })[0]);
    expect(screen.getByText("not recorded")).toBeInTheDocument();
  });

  it("filters by status and by text", async () => {
    installFetchMock({ "/api/v2/failures": envelope() });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    await userEvent.click(screen.getByLabelText("Status"));
    await userEvent.click(screen.getByRole("option", { name: "Warning" }));
    expect(screen.queryByText("check_drivesize")).not.toBeInTheDocument();
    expect(screen.getByText("check_cpu")).toBeInTheDocument();
    expect(screen.getByText("1/2")).toBeInTheDocument();

    await userEvent.click(screen.getByLabelText("Status"));
    await userEvent.click(screen.getByRole("option", { name: "All" }));
    await userEvent.type(screen.getByPlaceholderText("Filter failed checks"), "nrpe");
    expect(screen.queryByText("check_drivesize")).not.toBeInTheDocument();
    expect(screen.getByText("check_cpu")).toBeInTheDocument();
  });

  it("explains a switched-off log", async () => {
    installFetchMock({
      "/api/v2/failures": envelope({ enabled: false, count: 0, entries: [], max_entries: 0, statuses: "" }),
    });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText(/switched off/)).toBeInTheDocument());
    expect(screen.getByText("No failed checks recorded.")).toBeInTheDocument();
    expect(screen.getByRole("button", { name: /Clear all/ })).toBeDisabled();
  });

  it("clears the log over DELETE", async () => {
    let cleared = false;
    const fetchMock = installFetchMock({
      "/api/v2/failures": (request: Request) => {
        if (request.method === "DELETE") {
          cleared = true;
          return jsonResponse({ enabled: true, removed: 2 });
        }
        return cleared ? envelope({ count: 0, entries: [] }) : envelope();
      },
    });
    renderWithProviders(<FailedChecks />);

    await waitFor(() => expect(screen.getByText("check_drivesize")).toBeInTheDocument());
    await userEvent.click(screen.getByRole("button", { name: /Clear all/ }));

    await waitFor(() => expect(screen.getByText("No failed checks recorded.")).toBeInTheDocument());
    const deletes = fetchMock.mock.calls.filter((c) => (c[0] as Request).method === "DELETE");
    expect(deletes).toHaveLength(1);
  });
});
