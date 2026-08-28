import { describe, expect, it } from "vitest";
import { screen, waitFor } from "@testing-library/react";
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
    expect(
      chips[0].compareDocumentPosition(chips[1]) & Node.DOCUMENT_POSITION_FOLLOWING,
    ).toBeTruthy();
  });

  it("renders nothing while there are no tags", async () => {
    const fetchMock = installFetchMock({
      "/api/v2/tags": jsonResponse({}),
    });
    const { container } = renderWithProviders(<TagsWidget />, { withRouter: false });

    await waitFor(() => expect(fetchMock).toHaveBeenCalled());
    await waitFor(() => expect(container).toBeEmptyDOMElement());
  });
});
