// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

import { describe, expect, it } from "vitest";
import { QueryHelp } from "../api/api.ts";
import { emptyVocabulary, firstLine, makeVocabulary } from "./queryHelp.ts";

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
      name: "help",
      default_value: "",
      required: false,
      repeatable: false,
      content_type: "bool",
      short_description: "Show help",
      long_description: "Show help\nCommon option for all commands.",
    },
    {
      name: "show-all",
      default_value: "false",
      required: false,
      repeatable: false,
      content_type: "bool",
      short_description: "Show details for all matches",
      long_description: "Show details for all matches",
    },
  ],
  fields: [
    { name: "free", short_description: "", long_description: "Free space on the drive" },
    { name: "convert_bytes()", short_description: "", long_description: "Convert a byte value" },
    { name: "count", short_description: "", long_description: "Number of items. Common option for all checks." },
  ],
};

const vocabulary = makeVocabulary(help);
const option = (name: string) => vocabulary.options.find((o) => o.name === name)!;
const keyword = (name: string) => vocabulary.keywords.find((k) => k.name === name)!;

describe("makeVocabulary", () => {
  it("keeps a check's own options apart from the ones every check has", () => {
    expect(option("drive").common).toBe(false);
    expect(option("filter").common).toBe(true);
    expect(option("help").common).toBe(true);
  });

  it("takes the marker line off the description it shows", () => {
    expect(option("filter").description).toBe("Filter which items to include");
    expect(keyword("count").description).toBe("Number of items.");
  });

  it("marks the generic summary keywords as shared", () => {
    expect(keyword("count").common).toBe(true);
    expect(keyword("free").common).toBe(false);
  });

  it("drops the () the registry marks a filter function with", () => {
    expect(keyword("convert_bytes").kind).toBe("function");
    expect(keyword("convert_bytes").insert).toBe("convert_bytes(");
    expect(keyword("free").kind).toBe("keyword");
  });

  it("completes a flag to the spelling REST accepts", () => {
    // A bare `show-all` is refused over REST with "does not take any
    // arguments", so a flag completes to the form that works. `help` is a
    // switch and `show-all` a value<bool>; the agent reports both as bool.
    expect(option("help").insert).toBe("help=true");
    expect(option("show-all").insert).toBe("show-all=true");
    expect(option("drive").insert).toBe("drive=");
  });

  it("gives the highlighter the two name sets it checks against", () => {
    expect(vocabulary.vocabulary.variables.has("free")).toBe(true);
    expect(vocabulary.vocabulary.functions.has("convert_bytes")).toBe(true);
    expect(vocabulary.vocabulary.complete).toBe(true);
  });

  it("finds an option case insensitively and a keyword exactly", () => {
    expect(vocabulary.find("FILTER", "option")?.name).toBe("filter");
    expect(vocabulary.find("free", "keyword")?.name).toBe("free");
    expect(vocabulary.find("nope", "keyword")).toBeUndefined();
  });

  it("reports the keyword source, which an alias borrows from its target", () => {
    expect(vocabulary.keywordSource).toBe("check_drivesize");
    expect(makeVocabulary({ ...help, name: "my_alias", keyword_source: "check_drivesize" }).keywordSource).toBe(
      "check_drivesize",
    );
  });

  it("is empty, and says nothing, until the help has been fetched", () => {
    expect(makeVocabulary(undefined)).toBe(emptyVocabulary);
    expect(emptyVocabulary.vocabulary.complete).toBe(false);
  });
});

describe("firstLine", () => {
  it("keeps only the first line of a description", () => {
    expect(firstLine("one\ntwo")).toBe("one");
    expect(firstLine("one")).toBe("one");
  });
});
