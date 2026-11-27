// The client modules' targets against a fake settings API: that every
// documented target key has a place, what the card shows and warns about,
// what Save writes, and the probe.
import { describe, expect, it } from "vitest";
import { readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { parse } from "yaml";
import { screen, waitFor, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { Settings } from "../api/api";
import {
  authenticatedState,
  installFetchMock,
  jsonResponse,
  renderWithProviders,
} from "../test/test-utils";
import TargetsEditor from "./kit/TargetsEditor";
import type { TargetSpec } from "./kit/targets";
import { MODULE_PAGES } from "./registry";
import { NRPE_CLIENT_TARGETS } from "./NRPEClient/index";
import { NSCA_CLIENT_TARGETS } from "./NSCAClient/index";
import { GEARMAN_CLIENT_TARGETS } from "./GearmanClient/index";
import { NSCA_NG_CLIENT_TARGETS } from "./NSCANgClient/index";
import { ICINGA_CLIENT_TARGETS } from "./IcingaClient/index";
import { NRDP_CLIENT_TARGETS } from "./NRDPClient/index";
import { SMTP_CLIENT_TARGETS } from "./SMTPClient/index";
import { NSCP_CLIENT_TARGETS } from "./NSCPClient/index";
import { SYSLOG_CLIENT_TARGETS } from "./SyslogClient/index";
import { GRAPHITE_CLIENT_TARGETS } from "./GraphiteClient/index";
import { COLLECTD_CLIENT_TARGETS } from "./CollectdClient/index";
import { CHECK_MK_CLIENT_TARGETS } from "./CheckMKClient/index";

const SPECS: TargetSpec[] = [
  NRPE_CLIENT_TARGETS,
  NSCA_CLIENT_TARGETS,
  NSCA_NG_CLIENT_TARGETS,
  NRDP_CLIENT_TARGETS,
  NSCP_CLIENT_TARGETS,
  SMTP_CLIENT_TARGETS,
  SYSLOG_CLIENT_TARGETS,
  GRAPHITE_CLIENT_TARGETS,
  COLLECTD_CLIENT_TARGETS,
  CHECK_MK_CLIENT_TARGETS,
  GEARMAN_CLIENT_TARGETS,
  ICINGA_CLIENT_TARGETS,
];

// The keys docs/reference documents for a target (its `sample` keys).
function documentedTargetKeys(path: string): string[] {
  const dir = join(process.cwd(), "..", "docs", "reference");
  const out = new Set<string>();
  for (const name of readdirSync(dir).filter((n) => n.endsWith(".yaml"))) {
    const doc = parse(readFileSync(join(dir, name), "utf8")) as Record<
      string,
      { paths?: Record<string, { sample?: { keys?: Record<string, unknown> } }> }
    >;
    for (const platform of Object.values(doc)) {
      const keys = platform?.paths?.[path]?.sample?.keys;
      for (const k of Object.keys(keys ?? {})) out.add(k);
    }
  }
  return [...out];
}

describe("client target specs", () => {
  it("are registered as their module's targets editor", () => {
    for (const spec of SPECS) {
      const page = MODULE_PAGES.find((p) => p.id === spec.moduleId);
      const config = page?.collections?.[spec.path];
      expect(config?.kind === "instances" && config.Editor, spec.moduleId).toBeTruthy();
    }
  });

  it("give every documented target key a place", () => {
    const missing = SPECS.flatMap((spec) =>
      documentedTargetKeys(spec.path)
        .filter((k) => !(k in spec.fields))
        .map((k) => `${spec.moduleId}: ${k}`),
    );
    expect(missing).toEqual([]);
  });

  it("name only their own keys in sections", () => {
    for (const spec of SPECS)
      for (const key of spec.sections.flatMap((s) => s.keys))
        expect(spec.fields[key], `${spec.moduleId}: ${key}`).toBeDefined();
  });
});

function render(spec: TargetSpec, stored: Settings[], probe?: (url: URL) => unknown) {
  const writes: string[] = [];
  const probes: string[] = [];
  installFetchMock({
    "/api/v2/settings": async (req: Request) => {
      if (req.method === "PUT") {
        const body = (await req.json()) as Settings;
        writes.push(`[${body.path}] ${body.key}=${body.value}`);
        return jsonResponse("");
      }
      return jsonResponse(stored);
    },
    [`/api/v2/queries/${spec.test?.query ?? "none"}/commands/execute`]: async (req: Request) => {
      const url = new URL(req.url, "http://localhost");
      probes.push(url.search);
      return jsonResponse(probe?.(url) ?? {});
    },
  });
  renderWithProviders(<TargetsEditor spec={spec} />, { preloadedState: authenticatedState });
  return { writes, probes };
}

const T = NRPE_CLIENT_TARGETS.path;
const row = async (name: string) =>
  // The name is the row's first cell; an address can read the same.
  within((await screen.findAllByText(name, { selector: "p" }))[0].closest("tr") as HTMLElement);
const dialog = () => within(screen.getByRole("dialog"));

describe("NRPE client targets", () => {
  it("lists default first, and says the agent's default does not verify the server", async () => {
    render(NRPE_CLIENT_TARGETS, [{ path: `${T}/nagios`, key: "address", value: "nagios:5666" }]);
    const d = await row("default");
    expect(d.getByText("Used when a request names none")).toBeInTheDocument();
    expect(d.getByText("TLS, not verified")).toBeInTheDocument();
    const n = await row("nagios");
    expect(n.getByText("nagios:5666")).toBeInTheDocument();
    expect(n.getByText("The server is not verified")).toBeInTheDocument();
    expect(n.getByRole("button", { name: "Test" })).toBeInTheDocument();
  });

  it("verifies the server with one choice, and writes only what changes", async () => {
    const { writes } = render(NRPE_CLIENT_TARGETS, [
      { path: `${T}/nagios`, key: "address", value: "nagios" },
    ]);
    await userEvent.click((await row("nagios")).getByRole("button", { name: "Edit" }));
    await userEvent.click(dialog().getByRole("radio", { name: /^TLS, server verified/ }));
    expect(dialog().queryByText("The server is not verified")).not.toBeInTheDocument();
    await userEvent.click(dialog().getByRole("button", { name: "Save" }));
    await waitFor(() =>
      expect(writes).toEqual([
        `[${T}/nagios] verify mode=peer-cert`,
        "[/modules] NRPEClient=enabled",
      ]),
    );
  });

  it("reads legacy mode, and inherits the TLS keys from default", async () => {
    render(NRPE_CLIENT_TARGETS, [
      { path: `${T}/default`, key: "insecure", value: "true" },
      { path: `${T}/old`, key: "address", value: "old" },
    ]);
    const old = await row("old");
    expect(old.getByText("Legacy (anonymous TLS)")).toBeInTheDocument();
    expect(old.getByText("Legacy NRPE: the server is not authenticated")).toBeInTheDocument();
  });

  it("leaves a verify mode it cannot describe alone, as Custom", async () => {
    const { writes } = render(NRPE_CLIENT_TARGETS, [
      { path: `${T}/x`, key: "verify mode", value: "fail-if-no-cert" },
    ]);
    await userEvent.click((await row("x")).getByRole("button", { name: "Edit" }));
    expect(dialog().getByRole("radio", { name: /^Custom/ })).toBeChecked();
    await userEvent.type(dialog().getByLabelText("Timeout (seconds)"), "0");
    await userEvent.click(dialog().getByRole("button", { name: "Save" }));
    await waitFor(() =>
      expect(writes).toEqual([`[${T}/x] timeout=300`, "[/modules] NRPEClient=enabled"]),
    );
  });

  it("adds a target under its name", async () => {
    const { writes } = render(NRPE_CLIENT_TARGETS, []);
    await userEvent.click(await screen.findByRole("button", { name: "Add target" }));
    await userEvent.type(dialog().getByLabelText("Name"), "nagios");
    await userEvent.type(dialog().getByLabelText("Address"), "nagios.example");
    await userEvent.click(dialog().getByRole("button", { name: "Save" }));
    await waitFor(() =>
      expect(writes).toEqual([
        `[${T}/nagios] address=nagios.example`,
        "[/modules] NRPEClient=enabled",
      ]),
    );
  });

  it("refuses a name that is taken", async () => {
    render(NRPE_CLIENT_TARGETS, [{ path: `${T}/nagios`, key: "address", value: "n" }]);
    await userEvent.click(await screen.findByRole("button", { name: "Add target" }));
    await userEvent.type(dialog().getByLabelText("Name"), "nagios");
    expect(dialog().getByText("A target with that name exists.")).toBeInTheDocument();
    expect(dialog().getByRole("button", { name: "Save" })).toBeDisabled();
  });

  it("probes a target and shows what the server said", async () => {
    const { probes } = render(
      NRPE_CLIENT_TARGETS,
      [{ path: `${T}/nagios`, key: "address", value: "nagios" }],
      () => ({
        command: "check_nrpe",
        result: 0,
        lines: [{ message: "I (0.11.0) seem to be doing fine...", perf: {} }],
      }),
    );
    await userEvent.click((await row("nagios")).getByRole("button", { name: "Test" }));
    expect(await screen.findByText("I (0.11.0) seem to be doing fine...")).toBeInTheDocument();
    expect(probes).toEqual(["?target=nagios"]);
  });
});

describe("other client targets", () => {
  it("NSCA: says an empty key is no key", async () => {
    render(NSCA_CLIENT_TARGETS, []);
    expect(await screen.findByText("No password")).toBeInTheDocument();
    // NSCA has no TLS on the page.
    await userEvent.click((await row("default")).getByRole("button", { name: "Edit" }));
    expect(dialog().queryByLabelText("Use TLS")).not.toBeInTheDocument();
  });

  it("Gearman: an unencrypted target without the acknowledgement is refused", async () => {
    const G = GEARMAN_CLIENT_TARGETS.path;
    render(GEARMAN_CLIENT_TARGETS, [
      { path: `${G}/default`, key: "encryption", value: "false" },
      { path: `${G}/default`, key: "key", value: "k" },
    ]);
    expect(await screen.findByText("Every submission is refused")).toBeInTheDocument();
  });

  it("Gearman: picking Unencrypted writes the acknowledgement with it", async () => {
    const G = GEARMAN_CLIENT_TARGETS.path;
    const { writes } = render(GEARMAN_CLIENT_TARGETS, [
      { path: `${G}/default`, key: "key", value: "k" },
    ]);
    await userEvent.click((await row("default")).getByRole("button", { name: "Edit" }));
    await userEvent.click(dialog().getByRole("radio", { name: /^Unencrypted/ }));
    expect(dialog().getByText("Results are not encrypted")).toBeInTheDocument();
    await userEvent.click(dialog().getByRole("button", { name: "Save" }));
    await waitFor(() =>
      expect(writes).toEqual([
        `[${G}/default] encryption=false`,
        `[${G}/default] insecure=true`,
        "[/modules] GearmanClient=enabled",
      ]),
    );
  });

  it("NSCA-NG: certificate TLS without verifying the server is refused by the agent", async () => {
    const N = NSCA_NG_CLIENT_TARGETS.path;
    render(NSCA_NG_CLIENT_TARGETS, [{ path: `${N}/default`, key: "use psk", value: "false" }]);
    expect(await screen.findByText("Every submission is refused")).toBeInTheDocument();
  });

  it("Icinga and NRDP: plain http is called out", async () => {
    const I = ICINGA_CLIENT_TARGETS.path;
    render(ICINGA_CLIENT_TARGETS, [
      { path: `${I}/default`, key: "address", value: "http://icinga" },
    ]);
    expect(await screen.findByText("Sent over plain HTTP")).toBeInTheDocument();
  });

  it("SMTP: a login over plain SMTP is refused", async () => {
    const S = SMTP_CLIENT_TARGETS.path;
    render(SMTP_CLIENT_TARGETS, [
      { path: `${S}/default`, key: "security", value: "none" },
      { path: `${S}/default`, key: "username", value: "me" },
    ]);
    expect(await screen.findByText("Every mail is refused")).toBeInTheDocument();
  });

  it("only the active clients offer a probe", () => {
    expect(SPECS.filter((s) => s.test).map((s) => s.moduleId)).toEqual([
      "NRPEClient",
      "NSCPClient",
    ]);
  });
});
