import { describe, expect, it } from "vitest";
import { screen, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import Queries from "./Queries";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

const query = (name: string, plugin: string, description: string, experimental = false) => ({
  name,
  title: name,
  plugin,
  description,
  experimental,
  query_url: "",
});

const alias = (name: string, plugin: string, description: string, experimental = false) => ({
  name,
  title: name,
  plugin,
  description,
  experimental,
  alias_url: "",
});

function setup() {
  installFetchMock({
    "/api/v2/queries": jsonResponse([
      query("check_cpu", "CheckSystem", "Check the CPU load"),
      query("check_drivesize", "CheckDisk", "Check disk space"),
      query("check_temperature", "CheckSystem", "Check thermal zones", true),
      // Legacy alias form (checkXXX without underscore) must be hidden.
      query("checkcpu", "CheckSystem", "Legacy alias"),
    ]),
    "/api/v2/aliases": jsonResponse([
      alias("alias_cpu", "CheckSystem", "Alias for check_cpu"),
    ]),
  });
  return renderWithProviders(<Queries />);
}

describe("Queries page", () => {
  it("renders queries and aliases in separate tables", async () => {
    setup();

    expect(await screen.findByText("check_cpu")).toBeInTheDocument();
    expect(screen.getByText("check_drivesize")).toBeInTheDocument();
    expect(screen.getByText("alias_cpu")).toBeInTheDocument();
    expect(screen.getByText("Queries (3)")).toBeInTheDocument();
    expect(screen.getByText("Aliases (1)")).toBeInTheDocument();
  });

  it("marks the experimental query in the table", async () => {
    setup();
    await screen.findByText("check_cpu");

    const markers = screen.getAllByText("Experimental");
    expect(markers).toHaveLength(1);
    expect(markers[0].closest("tr")).toHaveTextContent("check_temperature");
  });

  it("hides legacy checkXXX aliases from the query list", async () => {
    setup();
    await screen.findByText("check_cpu");
    expect(screen.queryByText("checkcpu")).not.toBeInTheDocument();
  });

  it("filters both tables from the filter field", async () => {
    setup();
    await screen.findByText("check_cpu");

    await userEvent.type(screen.getByPlaceholderText("Filter checks..."), "drivesize");

    expect(screen.getByText("check_drivesize")).toBeInTheDocument();
    expect(screen.queryByText("check_cpu")).not.toBeInTheDocument();
    expect(screen.queryByText("alias_cpu")).not.toBeInTheDocument();
    expect(screen.getByText(/No aliases match/)).toBeInTheDocument();
  });

  it("filters on the experimental marker", async () => {
    setup();
    await screen.findByText("check_cpu");

    await userEvent.type(screen.getByPlaceholderText("Filter checks..."), "experimental");

    expect(screen.getByText("check_temperature")).toBeInTheDocument();
    expect(screen.queryByText("check_cpu")).not.toBeInTheDocument();
  });

  it("sorts queries when clicking a column header", async () => {
    setup();
    await screen.findByText("check_cpu");

    const queriesTable = screen.getAllByRole("table")[0];
    const nameHeader = within(queriesTable).getByRole("button", { name: "Name" });
    // The name column starts active/ascending, so one click flips it to descending.
    await userEvent.click(nameHeader);

    const rows = within(queriesTable).getAllByRole("row").slice(1);
    // The name cell also carries the experimental marker where there is one;
    // this test is about the order, so drop it before comparing.
    const names = rows.map((row) =>
      within(row).getAllByRole("cell")[0].textContent?.replace("Experimental", ""),
    );
    expect(names).toEqual(["check_temperature", "check_drivesize", "check_cpu"]);
  });
});
