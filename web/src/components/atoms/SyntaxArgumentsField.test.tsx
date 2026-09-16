// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

import { useState } from "react";
import { describe, expect, it, vi } from "vitest";
import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import SyntaxArgumentsField from "./SyntaxArgumentsField";
import { makeVocabulary } from "../../common/queryHelp";
import { QueryHelp } from "../../api/api";

const help: QueryHelp = {
  name: "check_drivesize",
  keyword_source: "check_drivesize",
  parameters: [
    {
      name: "drive",
      default_value: "",
      required: false,
      repeatable: true,
      content_type: "string",
      short_description: "The drives to check",
      long_description: "The drives to check",
    },
    {
      name: "filter",
      default_value: "none",
      required: false,
      repeatable: false,
      content_type: "string",
      short_description: "Filter which items to include",
      long_description: "Filter which items to include\nCommon option for all filter checks.",
    },
    {
      name: "show-all",
      default_value: "",
      required: false,
      repeatable: false,
      content_type: "bool",
      short_description: "Show details for all matches",
      long_description: "Show details for all matches\nCommon option for all filter checks.",
    },
  ],
  fields: [
    { name: "free", short_description: "", long_description: "Free space on the drive" },
    { name: "free_pct", short_description: "", long_description: "Free space as a percentage" },
    { name: "count", short_description: "", long_description: "Number of items. Common option for all checks." },
  ],
};

const vocabulary = makeVocabulary(help);

// The field is controlled, so a test needs something to hold the value.
function Harness({ onSubmit }: { onSubmit?: () => void } = {}) {
  const [value, setValue] = useState("");
  return <SyntaxArgumentsField value={value} onChange={setValue} vocabulary={vocabulary} onSubmit={onSubmit} />;
}

const field = () => screen.getByRole("textbox", { name: "Arguments" });

describe("SyntaxArgumentsField", () => {
  it("draws the typed line as coloured runs next to the editable copy", async () => {
    const user = userEvent.setup();
    render(<Harness />);
    await user.type(field(), "drive=c:");

    // The coloured layer is a copy of the text, not a second source of truth:
    // whatever is in the textarea has to be readable off it verbatim.
    const highlighted = document.querySelector("pre");
    expect(highlighted?.textContent?.trim()).toBe("drive=c:");
    expect(highlighted?.querySelectorAll("span")).toHaveLength(3);
    expect(field()).toHaveValue("drive=c:");
  });

  it("offers the check's own options where an option name is being typed", async () => {
    const user = userEvent.setup();
    render(<Harness />);
    await user.type(field(), "dri");

    expect(await screen.findByRole("menuitem", { name: /drive/ })).toBeInTheDocument();
    expect(screen.queryByRole("menuitem", { name: /free/ })).not.toBeInTheDocument();
  });

  it("offers the check's filter keywords inside a filter expression", async () => {
    const user = userEvent.setup();
    render(<Harness />);
    await user.type(field(), "filter=fre");

    expect(await screen.findByRole("menuitem", { name: /free_pct/ })).toBeInTheDocument();
    expect(screen.queryByRole("menuitem", { name: /drive.*The drives/ })).not.toBeInTheDocument();
  });

  it("completes the word under the caret, leaving the rest of the line alone", async () => {
    const user = userEvent.setup();
    render(<Harness />);
    await user.type(field(), "dri");
    await user.click(await screen.findByRole("menuitem", { name: /drive/ }));

    expect(field()).toHaveValue("drive=");
  });

  it("completes a flag to the form REST accepts, not to a bare name", async () => {
    const user = userEvent.setup();
    render(<Harness />);
    await user.type(field(), "show-a");
    await user.click(await screen.findByRole("menuitem", { name: /show-all/ }));

    // A bare `show-all` is rejected over REST with "does not take any
    // arguments"; `show-all=true` is the spelling that works on both
    // transports.
    expect(field()).toHaveValue("show-all=true");
  });

  it("runs the check on Enter rather than breaking the line", async () => {
    const onSubmit = vi.fn();
    const user = userEvent.setup();
    render(<Harness onSubmit={onSubmit} />);
    await user.type(field(), "drive=c:");
    await user.keyboard("{Escape}{Enter}");

    expect(onSubmit).toHaveBeenCalled();
    expect(field()).toHaveValue("drive=c:");
  });
});
