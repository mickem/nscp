import { describe, expect, it } from "vitest";
import { screen, waitFor } from "@testing-library/react";
import Router from "./Routes";
import {
  authenticatedState,
  installFetchMock,
  jsonResponse,
  renderWithProviders,
} from "./test/test-utils";

// Everything the authenticated shell (navbar, sidebar, dashboard) requests.
function mockShellApi() {
  return installFetchMock({
    "/api/v2/info": jsonResponse({ name: "NSClient++", version: "0.17.0", version_url: "" }),
    "/api/v2/logs/status": jsonResponse({ errors: 0, last_error: "" }),
    "/api/v2/settings/status": jsonResponse({ context: "ini", type: "ini", has_changed: false }),
    "/api/v2/metrics": jsonResponse({}),
    "/api/v2/tags": jsonResponse({}),
  });
}

describe("Router authentication gating", () => {
  it("shows the login page while unauthenticated", () => {
    mockShellApi();
    // Router brings its own BrowserRouter, so skip the MemoryRouter wrapper.
    renderWithProviders(<Router />, { withRouter: false });

    expect(screen.getByRole("heading", { name: "Sign in" })).toBeInTheDocument();
    expect(screen.queryByText("Dashboard")).not.toBeInTheDocument();
  });

  it("shows the dashboard shell once a token is present", async () => {
    mockShellApi();
    renderWithProviders(<Router />, {
      withRouter: false,
      preloadedState: authenticatedState,
    });

    // The refresh-rate selector is unique to the dashboard page; the nav
    // entries prove the sidebar rendered too.
    expect(await screen.findByLabelText("Refresh rate")).toBeInTheDocument();
    for (const item of ["Modules", "Settings", "Queries", "Metrics", "Events", "Logs", "About"]) {
      expect(screen.getAllByText(item).length).toBeGreaterThan(0);
    }
    expect(screen.queryByRole("heading", { name: "Sign in" })).not.toBeInTheDocument();
  });

  it("falls back to the login page when the API rejects the token", async () => {
    installFetchMock({
      "/api/v2/info": new Response("Forbidden", { status: 403 }),
      "/api/v2/logs/status": new Response("Forbidden", { status: 403 }),
      "/api/v2/settings/status": new Response("Forbidden", { status: 403 }),
      "/api/v2/metrics": new Response("Forbidden", { status: 403 }),
      "/api/v2/tags": new Response("Forbidden", { status: 403 }),
    });
    const { store } = renderWithProviders(<Router />, {
      withRouter: false,
      preloadedState: authenticatedState,
    });

    // A 403 dispatches removeToken which swaps the router back to the login page.
    await waitFor(() => {
      expect(store.getState().auth.token).toBeUndefined();
    });
    expect(await screen.findByRole("heading", { name: "Sign in" })).toBeInTheDocument();
  });
});
