import { describe, expect, it } from "vitest";
import { screen, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import Queries from "./Queries";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

const query = (name: string, plugin: string, description: string) => ({
  name,
  title: name,
  plugin,
  description,
  query_url: "",
});

const alias = (name: string, plugin: string, description: string) => ({
  name,
  title: name,
  plugin,
  description,
  alias_url: "",
});

function setup() {
  installFetchMock({
    "/api/v2/queries": jsonResponse([
      query("check_cpu", "CheckSystem", "Check the CPU load"),
      query("check_drivesize", "CheckDisk", "Check disk space"),
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
    expect(screen.getByText("Queries (2)")).toBeInTheDocument();
    expect(screen.getByText("Aliases (1)")).toBeInTheDocument();
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

  it("sorts queries when clicking a column header", async () => {
    setup();
    await screen.findByText("check_cpu");

    const queriesTable = screen.getAllByRole("table")[0];
    const nameHeader = within(queriesTable).getByRole("button", { name: "Name" });
    // The name column starts active/ascending, so one click flips it to descending.
    await userEvent.click(nameHeader);

    const rows = within(queriesTable).getAllByRole("row").slice(1);
    const names = rows.map((row) => within(row).getAllByRole("cell")[0].textContent);
    expect(names).toEqual(["check_drivesize", "check_cpu"]);
  });
});
