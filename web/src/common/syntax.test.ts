// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// The web half of modules/CommandClient/console_syntax_test.cpp: the same
// lines, painted the same way. Where a case here has a twin there, the two are
// meant to stay in step - a filter that lights up red in the browser must
// light up red at the prompt, and the other way round.

import { describe, expect, it } from "vitest";
import {
  classify,
  contextAt,
  filterCandidates,
  highlight,
  KeywordVocabulary,
  makeKeywordVocabulary,
  optionName,
  splitPoint,
  TokenKind,
  tokenize,
} from "./syntax.ts";

const kv: KeywordVocabulary = makeKeywordVocabulary([
  "free",
  "free_pct",
  "used",
  "drive",
  "total",
  "convert_size()",
  "auto_scale()",
]);

// The kinds covering `needle` in `input`, which must be one run of one kind.
const kindOf = (input: string, needle: string, vocabulary = kv): TokenKind => {
  const at = input.indexOf(needle);
  expect(at, `"${needle}" not in "${input}"`).toBeGreaterThanOrEqual(0);
  const kinds = classify(input, vocabulary);
  const run = new Set(kinds.slice(at, at + needle.length));
  expect(run.size, `"${needle}" is not one run: ${[...run].join()}`).toBe(1);
  return [...run][0];
};

describe("makeKeywordVocabulary", () => {
  it("splits the registry field list on the trailing ()", () => {
    expect(kv.variables.has("free")).toBe(true);
    expect(kv.variables.has("convert_size")).toBe(false);
    expect(kv.functions.has("convert_size")).toBe(true);
    expect(kv.complete).toBe(true);
  });

  it("stays incomplete for a check that declares no fields", () => {
    expect(makeKeywordVocabulary([]).complete).toBe(false);
  });
});

describe("tokenize", () => {
  it("splits on unquoted whitespace", () => {
    expect(tokenize("drive=c: filter=free < 10%").map((t) => t.text)).toEqual(["drive=c:", "filter=free", "<", "10%"]);
  });

  it("keeps a double-quoted run together, quotes included", () => {
    expect(tokenize('"filter=free < 10%" drive=c:').map((t) => t.text)).toEqual(['"filter=free < 10%"', "drive=c:"]);
  });

  it("runs an unterminated quote to the end of the line, which is what is on screen", () => {
    expect(tokenize('"filter=free <').map((t) => t.text)).toEqual(['"filter=free <']);
  });

  it("only opens a single-quoted string at the start of an argument or its value", () => {
    // The quote after the second '=' is an ordinary character, so this is one
    // token rather than the start of a string that swallows the rest.
    expect(tokenize("filter=core='total' drive=c:").map((t) => t.text)).toEqual(["filter=core='total'", "drive=c:"]);
  });
});

describe("splitPoint / optionName", () => {
  it("finds the first unquoted =", () => {
    expect(splitPoint("filter=free < 10%")).toBe(6);
    expect(splitPoint("show-all")).toBe(-1);
    expect(splitPoint("'a=b'")).toBe(-1);
  });

  it("reads --filter, -filter and filter as the same option", () => {
    expect(optionName("--Filter")).toBe("filter");
    expect(optionName("-filter")).toBe("filter");
    expect(optionName("filter")).toBe("filter");
  });
});

describe("classify", () => {
  it("paints an option name, its = and its value", () => {
    const input = "drive=c:";
    expect(kindOf(input, "drive")).toBe("option");
    expect(kindOf(input, "=")).toBe("punctuation");
    expect(kindOf(input, "c:")).toBe("value");
  });

  it("paints a bare flag as an option", () => {
    expect(kindOf("--show-all", "--show-all")).toBe("option");
  });

  it("reads a filter value as an expression", () => {
    const input = 'filter="free < 10%"';
    expect(kindOf(input, "free")).toBe("keyword");
    expect(kindOf(input, "<")).toBe("expressionOp");
    expect(kindOf(input, "10%")).toBe("number");
  });

  it("still sees an unquoted expression as several arguments", () => {
    // Arguments split on unquoted whitespace, so `filter=free < 10%` is three
    // of them and only the first is a filter - which is exactly the shape the
    // check itself sees, and why the expression wants quoting.
    const input = "filter=free < 10%";
    expect(kindOf(input, "free")).toBe("keyword");
    expect(kindOf(input, "<")).toBe("value");
    expect(kindOf(input, "10%")).toBe("value");
  });

  it("calls a keyword the check does not offer wrong", () => {
    expect(kindOf("filter=fre < 10%", "fre")).toBe("unknownKeyword");
  });

  it("says nothing about any name until the keywords have been fetched", () => {
    const empty = makeKeywordVocabulary([]);
    expect(kindOf("filter=fre < 10%", "fre", empty)).toBe("plain");
  });

  it("paints a filter function and the keyword in its argument", () => {
    const input = "warning=convert_size(free, 'B') < 10";
    expect(kindOf(input, "convert_size")).toBe("function");
    expect(kindOf(input, "free")).toBe("keyword");
    expect(kindOf(input, "'B'")).toBe("quoted");
  });

  it("paints the spelled-out operators of the expression language", () => {
    const input = "\"filter=drive like 'c' and free < 10\"";
    expect(kindOf(input, "like")).toBe("expressionOp");
    expect(kindOf(input, "and")).toBe("expressionOp");
  });

  it("reads a quoted filter as an expression rather than an opaque string", () => {
    const input = '"filter=free < 10%"';
    expect(kindOf(input, "filter")).toBe("option");
    expect(kindOf(input, "free")).toBe("keyword");
    expect(kindOf(input, "10%")).toBe("number");
  });

  it("leaves the literal text of a template alone and reads its placeholders", () => {
    const input = '"detail-syntax=${drive}: ${fre} free"';
    expect(kindOf(input, "${")).toBe("expressionOp");
    expect(kindOf(input, "drive")).toBe("keyword");
    expect(kindOf(input, "fre")).toBe("unknownKeyword");
    expect(kindOf(input, ": ")).toBe("value");
  });

  it("reads a %() placeholder the way the engine finds its closer", () => {
    const input = "\"top-syntax=%(status): %(convert_size(free, 'B'))\"";
    expect(kindOf(input, "convert_size")).toBe("function");
    expect(kindOf(input, "free")).toBe("keyword");
  });

  it("leaves perf-config alone - its keys are counters, not filter keywords", () => {
    expect(kindOf("perf-config=fre(unit:B)", "fre(unit:B)")).toBe("value");
  });

  it("says nothing about a dotted name, which is not a filter keyword at all", () => {
    expect(kindOf("detail-syntax=${column.fre}", "column.fre")).toBe("plain");
  });

  it("paints every character of the line", () => {
    const input = "drive=c: filter=free < 10%";
    expect(classify(input, kv)).toHaveLength(input.length);
  });

  it("merges into runs, in order, covering the whole line", () => {
    const input = "drive=c:";
    const spans = highlight(input, kv);
    expect(spans.map((s) => s.text).join("")).toBe(input);
    expect(spans.map((s) => s.kind)).toEqual(["option", "punctuation", "value"]);
  });
});

describe("contextAt", () => {
  it("offers options where an option name is being typed", () => {
    const ctx = contextAt("fil", 3);
    expect(ctx.target).toBe("option");
    expect(ctx.word).toBe("fil");
    expect([ctx.wordStart, ctx.wordEnd]).toEqual([0, 3]);
  });

  it("does not count the leading dashes as part of the name", () => {
    const ctx = contextAt("--sho", 5);
    expect(ctx.word).toBe("sho");
    expect(ctx.wordStart).toBe(2);
  });

  it("offers keywords inside a filter expression", () => {
    const input = "filter=fre";
    const ctx = contextAt(input, input.length);
    expect(ctx.target).toBe("keyword");
    expect(ctx.option).toBe("filter");
    expect(ctx.word).toBe("fre");
    expect([ctx.wordStart, ctx.wordEnd]).toEqual([7, 10]);
  });

  it("offers keywords inside a quoted filter expression", () => {
    const input = '"filter=free < 10% and dri"';
    const ctx = contextAt(input, input.length - 1);
    expect(ctx.target).toBe("keyword");
    expect(ctx.word).toBe("dri");
  });

  it("offers nothing in the value of an option that takes no keywords", () => {
    const ctx = contextAt("drive=c", 7);
    expect(ctx.target).toBe("none");
    expect(ctx.option).toBe("drive");
  });

  it("offers keywords only inside a placeholder of a template", () => {
    expect(contextAt("detail-syntax=${dr", 18).target).toBe("keyword");
    expect(contextAt('"detail-syntax=${drive}: fr"', 25).target).toBe("none");
    expect(contextAt("detail-syntax=%(dr", 18).target).toBe("keyword");
  });

  it("starts a new argument when the caret is past the last word", () => {
    const ctx = contextAt("drive=c: ", 9);
    expect(ctx.target).toBe("option");
    expect(ctx.word).toBe("");
  });

  it("completes the name, not the value, when the caret is before the =", () => {
    const ctx = contextAt("fil=free", 3);
    expect(ctx.target).toBe("option");
    expect(ctx.word).toBe("fil");
    expect(ctx.wordEnd).toBe(3);
  });
});

describe("filterCandidates", () => {
  const names = ["filter", "warning", "critical", "detail-syntax", "top-syntax"];
  const id = (s: string) => s;

  it("matches on the prefix, case insensitively", () => {
    expect(filterCandidates(names, "FIL", id)).toEqual(["filter"]);
  });

  it("falls back to matching anywhere only when nothing starts with the prefix", () => {
    expect(filterCandidates(names, "syntax", id)).toEqual(["detail-syntax", "top-syntax"]);
  });

  it("offers everything for an empty prefix", () => {
    expect(filterCandidates(names, "", id)).toHaveLength(names.length);
  });
});
