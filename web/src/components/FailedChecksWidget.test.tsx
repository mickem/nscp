import { describe, expect, it } from "vitest";
import { screen, waitFor } from "@testing-library/react";
import FailedChecksWidget from "./FailedChecksWidget";
import { formatAge } from "../common/formatAge";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

const envelope = (entries: unknown[]) =>
  jsonResponse({
    enabled: true,
    max_entries: 50,
    arguments_recorded: false,
    statuses: "warning,critical,unknown",
    last_index: entries.length,
    dropped: 0,
    count: entries.length,
    entries,
  });

const entry = (index: number, status: number, command: string, message: string, age = 30) => ({
  index,
  timestamp: 1760000000 + index,
  date: `2025-Oct-09 10:00:0${index}`,
  age,
  subject: "WEBServer:admin",
  depth: 0,
  command,
  alias: "",
  status,
  result: ["OK", "WARNING", "CRITICAL", "UNKNOWN"][status],
  message,
  lines: [{ message, perf: {} }],
});

describe("FailedChecksWidget", () => {
  it("lists the latest failures with their status, and links to the page", async () => {
    installFetchMock({
      "/api/v2/failures": envelope([
        entry(2, 2, "check_drivesize", "CRITICAL C: 92% used"),
        entry(1, 1, "check_cpu", "WARNING: 5m: 85%", 7200),
      ]),
    });
    renderWithProviders(<FailedChecksWidget />);

    await waitFor(() => expect(screen.getByText("Failed checks")).toBeInTheDocument());
    expect(screen.getByText("check_drivesize")).toBeInTheDocument();
    expect(screen.getByText("CRITICAL C: 92% used")).toBeInTheDocument();
    expect(screen.getByText("Critical")).toBeInTheDocument();
    expect(screen.getByText("check_cpu")).toBeInTheDocument();
    expect(screen.getByText("Warning")).toBeInTheDocument();
    expect(screen.getByText("2h ago")).toBeInTheDocument();
    expect(screen.getByRole("link", { name: "Show all" })).toHaveAttribute("href", "/failures");
  });

  it("asks for a preview, not the whole log", async () => {
    const fetchMock = installFetchMock({ "/api/v2/failures": envelope([]) });
    renderWithProviders(<FailedChecksWidget />);

    await waitFor(() => expect(fetchMock).toHaveBeenCalled());
    const request = fetchMock.mock.calls[0][0] as Request;
    expect(new URL(request.url, "http://localhost").search).toEqual("?limit=5");
  });

  it("renders nothing while nothing has failed", async () => {
    const fetchMock = installFetchMock({ "/api/v2/failures": envelope([]) });
    const { container } = renderWithProviders(<FailedChecksWidget />);

    await waitFor(() => expect(fetchMock).toHaveBeenCalled());
    await waitFor(() => expect(container).toBeEmptyDOMElement());
  });

  it("renders nothing on an agent without the endpoint", async () => {
    // An older core answers 404; the dashboard should not show an error card.
    const fetchMock = installFetchMock({});
    const { container } = renderWithProviders(<FailedChecksWidget />);

    await waitFor(() => expect(fetchMock).toHaveBeenCalled());
    await waitFor(() => expect(container).toBeEmptyDOMElement());
  });
});

describe("formatAge", () => {
  it.each([
    [0, "just now"],
    [4, "just now"],
    [42, "42s ago"],
    [300, "5m ago"],
    [10800, "3h ago"],
    [172800, "2d ago"],
  ])("formats %i seconds as %s", (seconds, text) => {
    expect(formatAge(seconds)).toEqual(text);
  });
});
