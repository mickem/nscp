import { describe, expect, it } from "vitest";
import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import FactTree from "./FactTree";
import { FactValue } from "../api/api";

const iface = (id: string, ip: string, status: string): FactValue => ({
  id,
  addresses: [ip, "fe80::8cc8:fb77:b612:7d30"],
  display_name: id,
  mac: "c0:bf:be:b7:e6:f5",
  speed_bps: 1000000000,
  status,
});

const interfaces = (count: number): FactValue =>
  Array.from({ length: count }, (_, i) => iface(`Ethernet ${i}`, `10.0.0.${i}`, "up"));

describe("FactTree", () => {
  it("renders a record list as one row each, with the fields folded away", async () => {
    // The point of the row: a dozen interfaces is a dozen lines, not the
    // seven-lines-each that made the card own the page.
    render(<FactTree node={{ interfaces: interfaces(4) }} />);

    expect(screen.getAllByRole("button")).toHaveLength(4);
    expect(screen.getByText("Ethernet 0")).toBeInTheDocument();
    // Folded: a field of the record is not on the page until the row opens.
    expect(screen.queryByText("speed_bps")).not.toBeInTheDocument();
  });

  it("puts the count on the heading of a list, since the rows are folded", () => {
    render(<FactTree node={{ interfaces: interfaces(12) }} />);

    expect(screen.getByText("interfaces (12)")).toBeInTheDocument();
  });

  it("opens one record's fields on a click, and closes it again", async () => {
    render(<FactTree node={{ interfaces: interfaces(3) }} />);

    const row = screen.getByRole("button", { name: /Ethernet 1/ });
    expect(row).toHaveAttribute("aria-expanded", "false");

    await userEvent.click(row);
    expect(row).toHaveAttribute("aria-expanded", "true");
    expect(screen.getByText("speed_bps:")).toBeInTheDocument();
    // Only that record opened - the other two are still rows.
    expect(screen.getAllByText("speed_bps:")).toHaveLength(1);

    await userEvent.click(row);
    expect(row).toHaveAttribute("aria-expanded", "false");
  });

  it("glances at a record's first short fields, and drops the glance once it is open", async () => {
    render(<FactTree node={{ interfaces: [iface("Ethernet", "10.0.0.14", "up")] }} />);

    // Alphabetical, as the opened record lists them: addresses (its first
    // one), display_name, mac. The values only - the keys are in the record.
    const row = screen.getByRole("button", { name: /Ethernet/ });
    expect(row.textContent).toContain("10.0.0.14 · Ethernet · c0:bf:be:b7:e6:f5");

    await userEvent.click(row);
    expect(row.textContent).not.toContain("10.0.0.14 · Ethernet");
  });

  it("keeps an identifier out of the glance, where it would crowd out the telling fields", () => {
    // A volume's device path is 44 characters of GUID and says nothing at a
    // glance; the three fields that do would be pushed off the row by it.
    render(
      <FactTree
        node={{
          volumes: [
            {
              id: "C:\\",
              device: "\\\\?\\Volume{7a093695-510e-11f1-a1c4-c0bfbeb7e6f5}\\",
              filesystem: "NTFS",
              label: "Samsung",
              type: "fixed",
            },
          ],
        }}
      />,
    );

    const row = screen.getByRole("button", { name: /C:/ });
    expect(row.textContent).toContain("NTFS · Samsung · fixed");
    expect(row.textContent).not.toContain("7a093695");
  });

  it("folds a long list between records rather than at a height, and says how many it held back", async () => {
    render(<FactTree node={{ interfaces: interfaces(15) }} />);

    expect(screen.getByText("Ethernet 11")).toBeInTheDocument();
    expect(screen.queryByText("Ethernet 12")).not.toBeInTheDocument();

    await userEvent.click(screen.getByRole("button", { name: "… 3 more" }));
    expect(screen.getByText("Ethernet 14")).toBeInTheDocument();

    await userEvent.click(screen.getByRole("button", { name: "Show fewer" }));
    expect(screen.queryByText("Ethernet 12")).not.toBeInTheDocument();
  });

  it("leaves a plain object as aligned key and value lines", () => {
    render(<FactTree node={{ family: "windows", version: "10.0.26200" }} />);

    expect(screen.getByText("family:")).toBeInTheDocument();
    expect(screen.getByText("windows")).toBeInTheDocument();
    expect(screen.queryByRole("button")).not.toBeInTheDocument();
  });

  it("renders a list of plain strings as lines, having no name to fold them under", () => {
    render(<FactTree node={{ addresses: ["10.0.0.14", "fe80::1"] }} />);

    expect(screen.getByText("10.0.0.14")).toBeInTheDocument();
    expect(screen.getByText("fe80::1")).toBeInTheDocument();
    expect(screen.queryByRole("button")).not.toBeInTheDocument();
  });

  it("says so when a set is empty, rather than rendering nothing", () => {
    render(<FactTree node={{ interfaces: [] }} />);

    expect(screen.getByText("(empty)")).toBeInTheDocument();
  });
});
