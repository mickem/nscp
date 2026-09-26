import { describe, expect, it } from "vitest";
import { screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import TagsWidget from "./TagsWidget";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

describe("TagsWidget", () => {
  it("renders one chip per tag, sorted by key", async () => {
    installFetchMock({
      "/api/v2/tags": jsonResponse({ os: "windows", drives: "C:,D:" }),
    });
    renderWithProviders(<TagsWidget />, { withRouter: false });

    await waitFor(() => {
      expect(screen.getByText("Tags")).toBeInTheDocument();
    });
    const chips = [screen.getByText("drives: C:,D:"), screen.getByText("os: windows")];
    chips.forEach((chip) => expect(chip).toBeInTheDocument());
    // Sorted alphabetically: drives before os.
    expect(chips[0].compareDocumentPosition(chips[1]) & Node.DOCUMENT_POSITION_FOLLOWING).toBeTruthy();
  });

  it("renders nothing while there are no tags", async () => {
    const fetchMock = installFetchMock({
      "/api/v2/tags": jsonResponse({}),
    });
    const { container } = renderWithProviders(<TagsWidget />, { withRouter: false });

    await waitFor(() => expect(fetchMock).toHaveBeenCalled());
    await waitFor(() => expect(container).toBeEmptyDOMElement());
  });

  it("folds a host that publishes more tags than the dashboard has room for", async () => {
    const many = Object.fromEntries(Array.from({ length: 30 }, (_, i) => [`tag${String(i).padStart(2, "0")}`, "yes"]));
    installFetchMock({ "/api/v2/tags": jsonResponse(many) });
    renderWithProviders(<TagsWidget />, { withRouter: false });

    await waitFor(() => expect(screen.getByText("Tags")).toBeInTheDocument());
    expect(screen.getByText("tag23: yes")).toBeInTheDocument();
    expect(screen.queryByText("tag24: yes")).not.toBeInTheDocument();

    await userEvent.click(screen.getByRole("button", { name: "… 6 more" }));
    expect(screen.getByText("tag29: yes")).toBeInTheDocument();
  });
});
