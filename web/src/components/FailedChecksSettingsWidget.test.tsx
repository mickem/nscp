import { describe, expect, it } from "vitest";
import { screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import FailedChecksSettingsWidget, { failedCheckSettings } from "./FailedChecksSettingsWidget";
import { SettingsDescription } from "../api/api";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

const description = (over: Partial<SettingsDescription> = {}): SettingsDescription => ({
  path: "/settings/core/failed checks",
  key: "enabled",
  type: "bool",
  title: "Record failed checks",
  description: "Keep the last `max entries` non-OK check results.",
  value: "",
  default_value: "true",
  icon: "",
  is_advanced_key: false,
  is_object: false,
  is_sample_key: false,
  is_template_key: false,
  plugins: [],
  sample_usage: "",
  ...over,
});

const descriptions: SettingsDescription[] = [
  // The section's own entry, which carries its title and no key.
  description({ key: "", type: "", title: "Failed-check log" }),
  description(),
  description({ key: "max entries", type: "int", title: "How many to keep", default_value: "50" }),
  description({
    key: "record arguments",
    type: "string",
    title: "Which of the check's arguments to record",
    default_value: "safe",
  }),
  // Not this section.
  description({ path: "/settings/core", key: "metrics interval", type: "string", title: "Metrics interval" }),
];

describe("failedCheckSettings", () => {
  it("takes the keys of the failed-check section, in registration order", () => {
    expect(failedCheckSettings(descriptions).map((d) => d.key)).toEqual(["enabled", "max entries", "record arguments"]);
  });
});

describe("FailedChecksSettingsWidget", () => {
  it("starts folded and shows every key once opened", async () => {
    installFetchMock({ "/api/v2/settings/descriptions": jsonResponse(descriptions) });
    renderWithProviders(<FailedChecksSettingsWidget />, { withRouter: false });

    const header = await screen.findByRole("button", { name: "Failed-check log settings" });
    expect(header).toHaveAttribute("aria-expanded", "false");
    expect(screen.queryByText("How many to keep")).not.toBeInTheDocument();

    await userEvent.click(header);
    expect(header).toHaveAttribute("aria-expanded", "true");
    expect(await screen.findByText("Record failed checks")).toBeInTheDocument();
    expect(screen.getByText("How many to keep")).toBeInTheDocument();
    expect(screen.getByText("Which of the check's arguments to record")).toBeInTheDocument();
    expect(screen.queryByText("Metrics interval")).not.toBeInTheDocument();
  });

  it("remembers that it was opened", async () => {
    installFetchMock({ "/api/v2/settings/descriptions": jsonResponse(descriptions) });
    const first = renderWithProviders(<FailedChecksSettingsWidget />, { withRouter: false });
    await userEvent.click(await screen.findByRole("button", { name: "Failed-check log settings" }));
    first.unmount();

    renderWithProviders(<FailedChecksSettingsWidget />, { withRouter: false });
    expect(await screen.findByRole("button", { name: "Failed-check log settings" })).toHaveAttribute(
      "aria-expanded",
      "true",
    );
  });

  it("renders nothing when the section cannot be read", async () => {
    installFetchMock({ "/api/v2/settings/descriptions": jsonResponse([]) });
    const { container } = renderWithProviders(<FailedChecksSettingsWidget />, { withRouter: false });
    await waitFor(() => expect(container).toBeEmptyDOMElement());
  });
});
