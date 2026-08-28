import { describe, expect, it, vi } from "vitest";
import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import NscpAlert from "./NscpAlert";

describe("NscpAlert", () => {
  it("renders the message text", () => {
    render(<NscpAlert severity="error" text="Something broke" />);
    const alert = screen.getByRole("alert");
    expect(alert).toHaveTextContent("Something broke");
  });

  it.each(["success", "info", "warning", "error"] as const)(
    "renders the %s severity styling",
    (severity) => {
      render(<NscpAlert severity={severity} text="msg" />);
      const capitalized = severity.charAt(0).toUpperCase() + severity.slice(1);
      expect(screen.getByRole("alert").className).toContain(capitalized);
    },
  );

  it("renders no action button without an actionTitle", () => {
    render(<NscpAlert severity="info" text="msg" />);
    expect(screen.queryByRole("button")).not.toBeInTheDocument();
  });

  it("fires onClick when the action button is pressed", async () => {
    const onClick = vi.fn();
    render(<NscpAlert severity="warning" text="msg" actionTitle="Retry" onClick={onClick} />);
    await userEvent.click(screen.getByRole("button", { name: "Retry" }));
    expect(onClick).toHaveBeenCalledTimes(1);
  });
});
