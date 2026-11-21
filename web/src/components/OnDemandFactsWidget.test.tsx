import { describe, expect, it } from "vitest";
import { screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import OnDemandFactsWidget from "./OnDemandFactsWidget";
import { FactSets, Facts } from "../api/api";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

const sets: FactSets = {
  scheduled: ["os"],
  on_demand: [
    { id: "pdh.instances", parameterized: true, privilege: "facts.get.pdh" },
    { id: "pdh.objects", parameterized: false, privilege: "facts.get.pdh" },
    { id: "scripts.python", parameterized: false, privilege: "facts.get.scripts" },
  ],
};

const reading = (over: Partial<Facts> = {}): Facts => ({
  revision: 3,
  collected: "2026-11-21T10:00:00Z",
  path: "scripts.python",
  found: true,
  enabled: ["os"],
  errors: {},
  gathered: { "scripts.python": "2026-11-21T10:00:05Z" },
  facts: { files: ["python/check_thing.py"] },
  on_demand: true,
  ...over,
});

async function expand() {
  // Folded by default: nothing in it is fetched until asked for.
  await waitFor(() => expect(screen.getByText("On demand")).toBeInTheDocument());
  await userEvent.click(screen.getByText("On demand"));
}

describe("OnDemandFactsWidget", () => {
  it("lists the on-demand sets with the privilege each takes, and fetches none of them", async () => {
    const fetchMock = installFetchMock({ "/api/v2/facts/sets": jsonResponse(sets) });
    renderWithProviders(<OnDemandFactsWidget />, { withRouter: false });
    await expand();

    expect(screen.getByText("scripts.python")).toBeInTheDocument();
    expect(screen.getByText("pdh.objects")).toBeInTheDocument();
    expect(screen.getByText("facts.get.scripts")).toBeInTheDocument();
    // One object at a time: not something a button can load.
    expect(screen.queryByText("pdh.instances")).not.toBeInTheDocument();
    // Only the catalog was asked for.
    expect(fetchMock.mock.calls.map(([input]) => new URL((input as Request).url).pathname)).toEqual(["/api/v2/facts/sets"]);
  });

  it("renders nothing when no module declares a set", async () => {
    installFetchMock({ "/api/v2/facts/sets": jsonResponse({ scheduled: [], on_demand: [] }) });
    const { container } = renderWithProviders(<OnDemandFactsWidget />, { withRouter: false });
    await waitFor(() => expect(container).toBeEmptyDOMElement());
  });

  it("collects a set when its button is pressed, by its path", async () => {
    const fetchMock = installFetchMock({
      "/api/v2/facts/sets": jsonResponse(sets),
      "/api/v2/facts": jsonResponse(reading()),
    });
    renderWithProviders(<OnDemandFactsWidget />, { withRouter: false });
    await expand();
    await userEvent.click(screen.getByRole("button", { name: "Load scripts.python" }));

    await waitFor(() => expect(screen.getByText("python/check_thing.py")).toBeInTheDocument());
    const asked = fetchMock.mock.calls.map(([input]) => new URL((input as Request).url)).find((url) => url.pathname === "/api/v2/facts");
    expect(asked?.searchParams.get("path")).toBe("scripts.python");
    // And it can be read again.
    expect(screen.getByRole("button", { name: "Load scripts.python" })).toHaveTextContent("Reload");
  });

  it("says which privilege a refused set takes, and keeps the session", async () => {
    installFetchMock({
      "/api/v2/facts/sets": jsonResponse(sets),
      "/api/v2/facts": new Response("403 requires facts.get.scripts", { status: 403 }),
    });
    const { store } = renderWithProviders(<OnDemandFactsWidget />, {
      withRouter: false,
      preloadedState: { auth: { token: "test-token", tokenInvalid: false } },
    });
    await expand();
    await userEvent.click(screen.getByRole("button", { name: "Load scripts.python" }));

    await waitFor(() => expect(screen.getByText(/Your role may not read this set/)).toBeInTheDocument());
    // A role that reads the inventory but not this listing is still logged in.
    expect(store.getState().auth.token).toBe("test-token");
  });

  it("shows the reason a set could not be collected", async () => {
    installFetchMock({
      "/api/v2/facts/sets": jsonResponse(sets),
      "/api/v2/facts": jsonResponse(reading({ found: false, facts: {}, gathered: {}, errors: { "scripts.python": "Failed to list /nowhere" } })),
    });
    renderWithProviders(<OnDemandFactsWidget />, { withRouter: false });
    await expand();
    await userEvent.click(screen.getByRole("button", { name: "Load scripts.python" }));

    await waitFor(() => expect(screen.getByText("Failed to list /nowhere")).toBeInTheDocument());
  });
});
