import { describe, expect, it } from "vitest";
import { useState } from "react";
import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import HostnameField from "./HostnameField";
import { describeHostname } from "./hostname";
import { findFieldWidget } from "../registry";

function Harness({ initial }: { initial: string }) {
  const [v, setV] = useState(initial);
  return (
    <>
      <HostnameField value={v} onChange={setV} />
      <output aria-label="value">{v}</output>
    </>
  );
}
const value = () => screen.getByLabelText("value").textContent;

describe("host name widget", () => {
  it("picks an auto form", async () => {
    render(<Harness initial="auto" />);
    expect(screen.getByRole("radio", { name: /as the system reports it/ })).toBeChecked();
    expect(screen.queryByLabelText("Host name")).not.toBeInTheDocument();
    await userEvent.click(screen.getByRole("radio", { name: /in lowercase/ }));
    expect(value()).toBe("auto-lc");
  });

  it("builds a name of its own from placeholders", async () => {
    render(<Harness initial="auto" />);
    await userEvent.click(screen.getByRole("radio", { name: "A name of its own" }));
    expect(value()).toBe("${host_lc}");
    await userEvent.click(screen.getByRole("button", { name: "Insert a placeholder" }));
    await userEvent.click(screen.getByText("${domain}"));
    expect(value()).toBe("${host_lc}${domain}");
    expect(screen.getByLabelText("Host name")).toHaveValue("${host_lc}${domain}");
  });

  it("reads a stored pattern as a name of its own", () => {
    render(<Harness initial="web-${host_lc}" />);
    expect(screen.getByRole("radio", { name: "A name of its own" })).toBeChecked();
    expect(screen.getByLabelText("Host name")).toHaveValue("web-${host_lc}");
  });

  it("describes a value in one line", () => {
    expect(describeHostname("auto")).toBe("The computer's name, as the system reports it.");
    expect(describeHostname("${host}.lan")).toBe("A pattern of the computer's name or address.");
    expect(describeHostname("box1")).toBe("A fixed name.");
  });

  it("is declared for every client section that reports a host name", () => {
    for (const path of [
      "/settings/NSCA/client",
      "/settings/NSCA-NG/client",
      "/settings/NRDP/client",
      "/settings/syslog/client",
      "/settings/graphite/client",
      "/settings/collectd/client",
      "/settings/gearman/client",
      "/settings/icinga/client",
      "/settings/elastic/client",
      "/settings/op5",
    ])
      expect(findFieldWidget(path, "hostname")?.kind, path).toBe("hostname");
  });
});
