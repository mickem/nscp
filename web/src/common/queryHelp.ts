// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// What `GET /api/v2/queries/<name>/help` answers, turned into what the
// argument editor and the help panel need: the completion candidates, the
// keyword vocabulary the highlighter checks names against, and the split
// between what this check defines and what every check shares.

import { QueryFieldHelp, QueryHelp, QueryParameterHelp } from "../api/api.ts";
import { KeywordVocabulary, makeKeywordVocabulary } from "./syntax.ts";

// Options and keywords shared by many commands carry a marker line appended to
// their description in C++ (modern_filter::cli_helper and
// nscapi::program_options::add_help for the options,
// parsers/where/filter_handler_impl.hpp for the generic summary keywords).
// The reference docs fold those out of every command's page and describe them
// once (scripts/python/docs_extract.py); the panel below does the same, so the
// handful of options this check actually defines are not buried under the
// thirty every check has. Keep these texts in step with the C++ and with
// docs_extract.py.
const OPTION_MARKERS = ["\nCommon option for all filter checks.", "\nCommon option for all commands."];
const FIELD_MARKER = "Common option for all checks.";

/** One thing the editor can complete and the panel can describe. */
export interface HelpEntry {
  name: string;
  /** Inserted when the entry is picked: `filter=` for an option, `free` for a keyword. */
  insert: string;
  kind: "option" | "keyword" | "function";
  /** The description with any common-option marker line taken off. */
  description: string;
  /** The value the check uses when the option is left out. Options only. */
  defaultValue?: string;
  /** "bool" for a flag, "string" otherwise. Options only. */
  contentType?: string;
  required?: boolean;
  /** True when this is shared by many checks rather than defined by this one. */
  common: boolean;
}

export interface QueryVocabulary {
  /** Everything `name=` can be, for completion in option position. */
  options: HelpEntry[];
  /** Everything a filter expression or a template placeholder can name. */
  keywords: HelpEntry[];
  /** The same keywords as the highlighter wants them: two name sets. */
  vocabulary: KeywordVocabulary;
  /** The command the keywords came from - an alias borrows its target's. */
  keywordSource: string;
  /** Looks an option or a keyword up by the name as typed. */
  find: (name: string, kind: "option" | "keyword") => HelpEntry | undefined;
}

function stripOptionMarker(description: string): { description: string; common: boolean } {
  for (const marker of OPTION_MARKERS) {
    if (description.endsWith(marker)) return { description: description.slice(0, -marker.length).trim(), common: true };
  }
  return { description: description.trim(), common: false };
}

function stripFieldMarker(description: string): { description: string; common: boolean } {
  if (description.endsWith(FIELD_MARKER)) {
    return { description: description.slice(0, -FIELD_MARKER.length).trim(), common: true };
  }
  return { description: description.trim(), common: false };
}

function optionEntry(p: QueryParameterHelp): HelpEntry {
  const { description, common } = stripOptionMarker(p.long_description || p.short_description || "");
  return {
    name: p.name,
    // A flag still takes a value here: a bare `show-all` is rejected over REST
    // with "does not take any arguments", so the editor offers the spelling
    // that works on the transport the page actually uses.
    insert: p.content_type === "bool" ? `${p.name}=true` : `${p.name}=`,
    kind: "option",
    description,
    defaultValue: p.default_value,
    contentType: p.content_type,
    required: p.required,
    common,
  };
}

function keywordEntry(f: QueryFieldHelp): HelpEntry {
  const { description, common } = stripFieldMarker(f.long_description || f.short_description || "");
  // The registry marks a filter function with a trailing "()" on its name.
  const isFunction = f.name.length > 2 && f.name.endsWith("()");
  const name = isFunction ? f.name.slice(0, -2) : f.name;
  return {
    name,
    insert: isFunction ? `${name}(` : name,
    kind: isFunction ? "function" : "keyword",
    description,
    common,
  };
}

/** Empty vocabulary, used until the help has been fetched. */
export const emptyVocabulary: QueryVocabulary = {
  options: [],
  keywords: [],
  vocabulary: makeKeywordVocabulary([]),
  keywordSource: "",
  find: () => undefined,
};

export function makeVocabulary(help: QueryHelp | undefined): QueryVocabulary {
  if (!help) return emptyVocabulary;
  const options = (help.parameters ?? []).map(optionEntry);
  const keywords = (help.fields ?? []).map(keywordEntry);
  const byOption = new Map(options.map((o) => [o.name.toLowerCase(), o]));
  const byKeyword = new Map(keywords.map((k) => [k.name, k]));
  return {
    options,
    keywords,
    vocabulary: makeKeywordVocabulary((help.fields ?? []).map((f) => f.name)),
    keywordSource: help.keyword_source || help.name,
    find: (name, kind) => (kind === "option" ? byOption.get(name.toLowerCase()) : byKeyword.get(name)),
  };
}

/** The first line of a description - all there is room for in a one-line hint. */
export function firstLine(description: string): string {
  const eol = description.indexOf("\n");
  return eol < 0 ? description : description.slice(0, eol);
}
