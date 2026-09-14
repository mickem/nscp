import { describe, expect, it } from "vitest";
import { screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import AppNavbar from "./AppNavbar";
import {
  authenticatedState,
  installFetchMock,
  jsonResponse,
  renderWithProviders,
} from "../test/test-utils";

function setup(logStatus: { errors: number; last_error: string }) {
  const logoutCalls: Array<{ method: string; authorization: string | null }> = [];
  installFetchMock({
    "/api/v2/info": jsonResponse({ name: "NSClient++", version: "0.17.0", version_url: "" }),
    "/api/v2/logs/status": jsonResponse(logStatus),
    "/api/v2/login": (req: Request) => {
      logoutCalls.push({ method: req.method, authorization: req.headers.get("authorization") });
      return jsonResponse({ status: "ok" });
    },
  });
  return {
    logoutCalls,
    ...renderWithProviders(<AppNavbar handleDrawerToggle={() => {}} />, {
      preloadedState: authenticatedState,
    }),
  };
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
    localStorage.setItem("token", "test-token");

    await userEvent.click(screen.getByRole("button", { name: "account of current user" }));
    await userEvent.click(await screen.findByRole("menuitem", { name: "Logout" }));

    expect(store.getState().auth.token).toBeUndefined();
    // Not just forgotten by this browser: the persisted copy goes too, rather
    // than sitting in localStorage until something happens to notice it.
    expect(localStorage.getItem("token")).toBeNull();
  });

  it("revokes the session token on the server when logging out", async () => {
    // Clearing the client state only makes this browser forget the bearer;
    // the server accepts it for its full eight-hour life unless it is
    // revoked, so a copy taken from a shared machine or a proxy log would
    // still work after the admin logged out.
    const { logoutCalls } = setup({ errors: 0, last_error: "" });

    await userEvent.click(screen.getByRole("button", { name: "account of current user" }));
    await userEvent.click(await screen.findByRole("menuitem", { name: "Logout" }));

    await waitFor(() => expect(logoutCalls).toHaveLength(1));
    expect(logoutCalls[0].method).toBe("DELETE");
    // Sent while the token is still in the store: the server has to be told
    // which token to revoke.
    expect(logoutCalls[0].authorization).toBe("Bearer test-token");
  });

  it("still logs out locally when the revoke call fails", async () => {
    installFetchMock({
      "/api/v2/info": jsonResponse({ name: "NSClient++", version: "0.17.0", version_url: "" }),
      "/api/v2/logs/status": jsonResponse({ errors: 0, last_error: "" }),
      "/api/v2/login": new Response("Forbidden", { status: 403 }),
    });
    const { store } = renderWithProviders(<AppNavbar handleDrawerToggle={() => {}} />, {
      preloadedState: authenticatedState,
    });

    await userEvent.click(screen.getByRole("button", { name: "account of current user" }));
    await userEvent.click(await screen.findByRole("menuitem", { name: "Logout" }));

    await waitFor(() => expect(store.getState().auth.token).toBeUndefined());
  });
});
