import { describe, expect, it } from "vitest";
import { screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import Login from "./Login";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

const loginRoute = (validPassword: string) => ({
  "/api/v2/login": (req: Request) => {
    const auth = req.headers.get("authorization") || "";
    const expected = `Basic ${btoa(`admin:${validPassword}`)}`;
    if (auth === expected) {
      return jsonResponse({ key: "session-key-123" });
    }
    return new Response("Forbidden", { status: 403 });
  },
});

describe("Login page", () => {
  it("renders branding, credential fields and the sign-in button", () => {
    installFetchMock({});
    renderWithProviders(<Login />, { withRouter: false });

    expect(screen.getByText("NSClient++")).toBeInTheDocument();
    expect(screen.getByRole("heading", { name: "Sign in" })).toBeInTheDocument();
    expect(screen.getByLabelText(/username/i)).toBeInTheDocument();
    expect(screen.getByLabelText(/password/i)).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Sign in" })).toBeInTheDocument();
    expect(screen.queryByRole("alert")).not.toBeInTheDocument();
  });

  it("masks the password input", () => {
    installFetchMock({});
    renderWithProviders(<Login />, { withRouter: false });
    expect(screen.getByLabelText(/password/i)).toHaveAttribute("type", "password");
  });

  it("shows an error alert when the credentials are rejected", async () => {
    installFetchMock(loginRoute("right"));
    renderWithProviders(<Login />, { withRouter: false });

    await userEvent.type(screen.getByLabelText(/username/i), "admin");
    await userEvent.type(screen.getByLabelText(/password/i), "wrong");
    await userEvent.click(screen.getByRole("button", { name: "Sign in" }));

    const alert = await screen.findByRole("alert");
    expect(alert).toHaveTextContent("Login failed. Please check your credentials and try again.");
  });

  it("stores the token on a successful login", async () => {
    installFetchMock(loginRoute("s3cret"));
    const { store } = renderWithProviders(<Login />, { withRouter: false });

    await userEvent.type(screen.getByLabelText(/username/i), "admin");
    await userEvent.type(screen.getByLabelText(/password/i), "s3cret");
    await userEvent.click(screen.getByRole("button", { name: "Sign in" }));

    await waitFor(() => {
      expect(store.getState().auth.token).toBe("session-key-123");
    });
    expect(localStorage.getItem("token")).toBe("session-key-123");
    expect(screen.queryByRole("alert")).not.toBeInTheDocument();
  });

  it("submits the form when pressing Enter in a field", async () => {
    installFetchMock(loginRoute("s3cret"));
    const { store } = renderWithProviders(<Login />, { withRouter: false });

    await userEvent.type(screen.getByLabelText(/username/i), "admin");
    await userEvent.type(screen.getByLabelText(/password/i), "s3cret{Enter}");

    await waitFor(() => {
      expect(store.getState().auth.token).toBe("session-key-123");
    });
  });

  it("restores a previously stored token from localStorage", async () => {
    installFetchMock({});
    localStorage.setItem("token", "stored-token");
    const { store } = renderWithProviders(<Login />, { withRouter: false });

    await waitFor(() => {
      expect(store.getState().auth.token).toBe("stored-token");
    });
  });
});
