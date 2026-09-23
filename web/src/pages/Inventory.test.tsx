import { describe, expect, it } from "vitest";
import { screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import Inventory from "./Inventory";
import { installFetchMock, jsonResponse, renderWithProviders } from "../test/test-utils";

// sha256 of "{}" — what an agent with nothing enabled reports.
const EMPTY_HASH = "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a";

const EMPTY = {
  revision: 0,
  hash: EMPTY_HASH,
  enabled: [],
  errors: {},
  facts: {},
};

const COLLECTED = {
  revision: 7,
  hash: "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
  enabled: ["os", "storage.volumes"],
  errors: {},
  facts: {
    os: { family: "linux", name: "Ubuntu 24.04", version: "6.8.0" },
    storage: {
      volumes: [
        { id: "/", fs: "ext4", size_bytes: 255000000000 },
        { id: "/boot", fs: "vfat", size_bytes: 1000000000 },
      ],
    },
  },
};

function setup(body: unknown) {
  installFetchMock({
    "/api/v2/facts": jsonResponse(body),
  });
  return renderWithProviders(<Inventory />);
}

describe("Inventory page", () => {
  // Nothing enabled is the state every fresh install is in, so the page has
  // to explain it rather than render as an empty screen that reads as a bug.
  it("explains how to turn inventory on when nothing is enabled", async () => {
    setup(EMPTY);

    expect(await screen.findByText("No inventory is being collected")).toBeInTheDocument();
    expect(screen.getByText(/\[\/settings\/facts\]/)).toBeInTheDocument();
    expect(screen.getByText(/facts list/)).toBeInTheDocument();
  });

  it("renders each fact set, with a record list as a table", async () => {
    setup(COLLECTED);

    // By heading, not by text: `os` is also one of the enabled-set chips in
    // the toolbar.
    expect(await screen.findByRole("heading", { name: "os" })).toBeInTheDocument();
    expect(screen.getByText("Ubuntu 24.04")).toBeInTheDocument();
    expect(screen.getByRole("heading", { name: "storage" })).toBeInTheDocument();
    // The records of storage.volumes, one row each, with their id column.
    expect(screen.getByText("/")).toBeInTheDocument();
    expect(screen.getByText("/boot")).toBeInTheDocument();
    expect(screen.getByText("ext4")).toBeInTheDocument();
  });

  it("names the enabled fact sets and the document revision", async () => {
    setup(COLLECTED);

    expect(await screen.findByText("storage.volumes")).toBeInTheDocument();
    expect(screen.getByText("revision 7")).toBeInTheDocument();
  });

  it("filters on what is inside a set, not only on its name", async () => {
    setup(COLLECTED);
    await screen.findByRole("heading", { name: "os" });

    await userEvent.type(screen.getByPlaceholderText(/Filter inventory/), "ext4");

    expect(screen.getByRole("heading", { name: "storage" })).toBeInTheDocument();
    expect(screen.queryByRole("heading", { name: "os" })).not.toBeInTheDocument();
    expect(screen.queryByText("Ubuntu 24.04")).not.toBeInTheDocument();
  });

  it("reports a fact set that could not be collected instead of leaving it out silently", async () => {
    setup({
      ...COLLECTED,
      errors: { "software.installed": "access denied to HKLM\\SOFTWARE" },
    });

    expect(await screen.findByText("Some fact sets could not be collected")).toBeInTheDocument();
    expect(screen.getByText(/access denied/)).toBeInTheDocument();
  });
});
