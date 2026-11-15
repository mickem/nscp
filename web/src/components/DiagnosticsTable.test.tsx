import { describe, expect, it } from "vitest";
import { render, screen, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import DiagnosticsTable from "./DiagnosticsTable";
import { DiagnosticsSection } from "../api/api";

const CAPTURED = "2026-11-15T10:42:06Z";
// 42 seconds after the capture.
const NOW = Date.parse(CAPTURED) + 42_000;

const TOP_CPU: DiagnosticsSection = {
  source: "CheckSystem",
  description: "The processes using the most CPU over the last second",
  captured_at: CAPTURED,
  truncated: true,
  columns: [
    { name: "name", type: "string" },
    { name: "pid", type: "int" },
    { name: "cpu", type: "float", unit: "percent", description: "Share of all cores over the last second" },
    { name: "working_set", type: "int", unit: "bytes", description: "Resident memory" },
    { name: "private_bytes", type: "int", unit: "bytes" },
  ],
  rows: [
    { name: "acme-indexer.exe", pid: 4242, cpu: 87.5, working_set: 412000256, private_bytes: 398458880 },
    { name: "svchost.exe", pid: 1288, cpu: 2.125, working_set: 85983232, private_bytes: null },
  ],
};

const LONG_MESSAGE =
  "The acme-sync service terminated unexpectedly. It has done this 4 time(s). The following corrective action " +
  "will be taken in 60000 milliseconds: Restart the service.";

const EVENTS: DiagnosticsSection = {
  source: "CheckEventLog",
  captured_at: CAPTURED,
  truncated: false,
  columns: [
    { name: "time", type: "timestamp" },
    { name: "service", type: "string" },
    { name: "id", type: "int" },
    { name: "message", type: "string" },
    { name: "restarted", type: "bool" },
  ],
  rows: [{ time: "2026-11-15T10:41:52Z", service: "acme-sync", id: 7031, message: LONG_MESSAGE, restarted: true }],
};

describe("DiagnosticsTable", () => {
  it("renders the header and the rows in column order", () => {
    render(<DiagnosticsTable name="processes.top_cpu" section={TOP_CPU} now={NOW} />);

    expect(screen.getByRole("heading", { name: "processes.top_cpu" })).toBeInTheDocument();
    expect(screen.getByText("CheckSystem")).toBeInTheDocument();
    expect(screen.getByText(TOP_CPU.description!)).toBeInTheDocument();
    expect(screen.getByText(`captured 42s ago · ${new Date(CAPTURED).toLocaleString()}`)).toBeInTheDocument();

    const table = screen.getByRole("table", { name: "processes.top_cpu rows" });
    const headers = within(table)
      .getAllByRole("columnheader")
      .map((h) => h.textContent);
    expect(headers).toEqual(["name", "pid", "cpu%", "working_setbytes", "private_bytesbytes"]);

    const rows = within(table).getAllByRole("row");
    const cells = within(rows[1])
      .getAllByRole("cell")
      .map((c) => c.textContent);
    expect(cells).toEqual(["acme-indexer.exe", "4242", "87.5 %", "392.9 MiB", "380.0 MiB"]);
    // A null cell is a plain placeholder, not an empty or zero value.
    expect(within(rows[2]).getAllByRole("cell")[4]).toHaveTextContent(/^-$/);
  });

  it("says the table was cut by a cap", () => {
    render(<DiagnosticsTable name="processes.top_cpu" section={TOP_CPU} now={NOW} />);
    expect(screen.getByText("showing the first 2")).toBeInTheDocument();
  });

  it("shows a column's description as its header tooltip", async () => {
    render(<DiagnosticsTable name="processes.top_cpu" section={TOP_CPU} now={NOW} />);
    await userEvent.hover(screen.getByText("working_set"));
    expect(await screen.findByRole("tooltip")).toHaveTextContent("Resident memory (bytes)");
  });

  it("formats timestamps and booleans, and clamps long text with the full text in its title", async () => {
    render(<DiagnosticsTable name="services.events" section={EVENTS} now={NOW} />);

    expect(screen.queryByText(/showing the first/)).not.toBeInTheDocument();
    expect(screen.getByText(new Date("2026-11-15T10:41:52Z").toLocaleString())).toHaveAttribute(
      "title",
      "2026-11-15T10:41:52Z",
    );
    expect(screen.getByText("yes")).toBeInTheDocument();
    const message = screen.getByText(LONG_MESSAGE);
    expect(message).toHaveAttribute("title", LONG_MESSAGE);
    expect(message).toHaveStyle({ overflow: "hidden" });
    await userEvent.click(message);
    expect(message).toHaveStyle({ whiteSpace: "pre-wrap" });
  });

  it("shows a section that could not be collected as an error, not as an empty table", () => {
    render(
      <DiagnosticsTable
        name="eventlog.errors"
        section={{ ...EVENTS, rows: [], error: "Access is denied." }}
        now={NOW}
      />,
    );
    expect(screen.getByText("Could not be collected: Access is denied.")).toBeInTheDocument();
    expect(screen.queryByRole("table")).not.toBeInTheDocument();
  });

  it("says there are no rows rather than drawing an empty grid", () => {
    render(<DiagnosticsTable name="services.problems" section={{ ...EVENTS, rows: [] }} now={NOW} />);
    expect(screen.getByText(/No rows/)).toBeInTheDocument();
    expect(screen.queryByRole("table")).not.toBeInTheDocument();
  });
});
