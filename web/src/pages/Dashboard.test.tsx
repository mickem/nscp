import { describe, expect, it } from "vitest";
import { screen, waitFor } from "@testing-library/react";
import Dashboard from "./Dashboard";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

describe("Dashboard page", () => {
  it("renders the heading and refresh selector without any metrics", async () => {
    const fetchMock = installFetchMock({
      "/api/v2/metrics": jsonResponse({}),
      "/api/v2/tags": jsonResponse({}),
    });
    renderWithProviders(<Dashboard />);

    expect(screen.getByText("Dashboard")).toBeInTheDocument();
    expect(screen.getByLabelText("Refresh rate")).toBeInTheDocument();
    await waitFor(() => expect(fetchMock).toHaveBeenCalled());
    // No data: none of the widgets may render.
    expect(screen.queryByText("CPU Load")).not.toBeInTheDocument();
    expect(screen.queryByText("Memory Usage")).not.toBeInTheDocument();
    expect(screen.queryByText("System Info")).not.toBeInTheDocument();
  });

  it("renders the widgets matching the metrics the server reports", async () => {
    installFetchMock({
      "/api/v2/metrics": jsonResponse({
        "system.cpu.total.kernel": 12.5,
        "system.cpu.total.user": 30.1,
        "system.mem.physical.used": 4 * 1024 ** 3,
        "system.mem.physical.total": 16 * 1024 ** 3,
        "system.uptime.uptime": "3d 2:07",
        "workers.jobs": 7,
        "workers.refresh_interval": 10,
        "system.refresh_interval": 10,
        "disk.free.C:.total": 100 * 1024 ** 3,
        "disk.free.C:.free": 40 * 1024 ** 3,
        "disk.free.C:.used": 60 * 1024 ** 3,
        "disk.free.C:.used_pct": 60,
      }),
      "/api/v2/tags": jsonResponse({ drives: "C:" }),
    });
    renderWithProviders(<Dashboard />);

    expect(await screen.findByText("CPU Load")).toBeInTheDocument();
    expect(screen.getByText("Memory Usage")).toBeInTheDocument();
    expect(screen.getByText("Disk Space")).toBeInTheDocument();
    expect(screen.getByText("System Info")).toBeInTheDocument();
    expect(screen.getByRole("row", { name: /Uptime 3d 2:07/ })).toBeInTheDocument();
    expect(await screen.findByText("Tags")).toBeInTheDocument();
    expect(screen.getByText("drives: C:")).toBeInTheDocument();
  });
});
