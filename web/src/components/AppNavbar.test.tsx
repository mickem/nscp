import { describe, expect, it } from "vitest";
import { screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import AppNavbar from "./AppNavbar";
import {
  authenticatedState,
  installFetchMock,
  jsonResponse,
  renderWithProviders,
} from "../test/test-utils";

function setup(logStatus: { errors: number; last_error: string }) {
  installFetchMock({
    "/api/v2/info": jsonResponse({ name: "NSClient++", version: "0.17.0", version_url: "" }),
    "/api/v2/logs/status": jsonResponse(logStatus),
  });
  return renderWithProviders(<AppNavbar handleDrawerToggle={() => {}} />, {
    preloadedState: authenticatedState,
  });
}

describe("AppNavbar", () => {
  it("shows the product name and the server version", async () => {
    setup({ errors: 0, last_error: "" });

    expect(screen.getByText("NSClient++")).toBeInTheDocument();
    expect(await screen.findByText("0.17.0")).toBeInTheDocument();
  });

  it("hides the error badge while the log is clean", async () => {
    setup({ errors: 0, last_error: "" });
    await screen.findByText("0.17.0");
    expect(screen.queryByText("3")).not.toBeInTheDocument();
  });

  it("shows an error badge with the error count when the log has errors", async () => {
    setup({ errors: 3, last_error: "something failed" });
    expect(await screen.findByText("3")).toBeInTheDocument();
  });

  it("opens the account menu with a logout entry", async () => {
    setup({ errors: 0, last_error: "" });

    await userEvent.click(screen.getByRole("button", { name: "account of current user" }));
    expect(await screen.findByRole("menuitem", { name: "Logout" })).toBeInTheDocument();
  });

  it("clears the session when logging out", async () => {
    const { store } = setup({ errors: 0, last_error: "" });
    expect(store.getState().auth.token).toBe("test-token");

    await userEvent.click(screen.getByRole("button", { name: "account of current user" }));
    await userEvent.click(await screen.findByRole("menuitem", { name: "Logout" }));

    expect(store.getState().auth.token).toBeUndefined();
  });
});
