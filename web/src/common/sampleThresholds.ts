// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

// Simple mode on the query page: a pair of sample thresholds written into the
// argument line, so that someone who has never seen the filter language has a
// working `warning=` and `critical=` to change a number in, instead of an
// empty line and a grammar to learn.
//
// The sample is the check's own default wherever it has one - the values a
// check falls back to are the ones its author considered sensible, and writing
// them out changes nothing about the result until they are edited. A few
// checks have no default (they are informational until told what to alert on)
// or have one the help cannot carry faithfully; those get a sample from the
// table below.

import { QueryVocabulary } from "./queryHelp.ts";

/** The expressions to offer: one `warning=` / `critical=` argument per entry. */
export interface SampleThresholds {
  warning: string[];
  critical: string[];
  /** Where the sample came from, for the line that explains it. */
  source: "defaults" | "sample";
}

interface CuratedSample {
  warning: string[];
  critical: string[];
  /** Keywords the expressions use. A check that lacks one gets no sample. */
  requires: string[];
}

// Keyed by command name, lower case. Every keyword named here is checked
// against what the check reports before the sample is offered, so an entry can
// never put an expression on the line that the check would reject - the same
// command name is a different implementation on Windows and on Unix.
const CURATED: Record<string, CuratedSample> = {
  // Two critical defaults. The help joins them as "a, b", which cannot be
  // split again: a comma is also what separates the values of `in (...)`.
  check_process: {
    warning: ["state not in ('started')"],
    critical: ["state = 'stopped'", "count = 0"],
    requires: ["state"],
  },
  // Two warning defaults, for the same reason.
  check_eventlog: {
    warning: ["level = 'warning'", "problem_count > 0"],
    critical: ["level in ('error', 'critical')"],
    requires: ["level"],
  },
  // No default: what a high load is depends on the number of cores. These suit
  // a small server and are meant to be edited.
  check_load: {
    warning: ["load5 > 4"],
    critical: ["load5 > 8"],
    requires: ["load5"],
  },
  // No default: any sustained swapping out is worth a look, a lot of it is a
  // machine short of memory.
  check_swap_io: {
    warning: ["swap_out > 100"],
    critical: ["swap_out > 1000"],
    requires: ["swap_out"],
  },
};

/**
 * The sample thresholds for a check, or undefined when there is nothing
 * sensible to offer: the check takes no thresholds, or has no default and no
 * entry in the table.
 */
export function sampleThresholds(queryName: string, vocabulary: QueryVocabulary): SampleThresholds | undefined {
  const warningOption = vocabulary.find("warning", "option");
  const criticalOption = vocabulary.find("critical", "option");
  // Not a filter check (a script, a wrapped command): nothing to write.
  if (!warningOption && !criticalOption) return undefined;

  const curated = CURATED[queryName.toLowerCase()];
  if (curated && curated.requires.every((keyword) => vocabulary.find(keyword, "keyword"))) {
    return { warning: curated.warning, critical: curated.critical, source: "sample" };
  }

  const warning = warningOption?.defaultValue ? [warningOption.defaultValue] : [];
  const critical = criticalOption?.defaultValue ? [criticalOption.defaultValue] : [];
  if (warning.length === 0 && critical.length === 0) return undefined;
  return { warning, critical, source: "defaults" };
}

const TOKEN_REGEXP = /\\?.|^$/g;

/**
 * Splits the argument line into the arguments sent to the check: on spaces,
 * with `"` grouping and `\` taking the next character literally.
 */
export function splitArguments(line: string): string[] {
  return (
    line.match(TOKEN_REGEXP)?.reduce(
      (p, c) => {
        if (c === '"') {
          p.quote ^= 1;
        } else if (!p.quote && c === " ") {
          p.a.push("");
        } else {
          p.a[p.a.length - 1] += c.replace(/\\(.)/, "$1");
        }
        return p;
      },
      { a: [""], quote: 0 },
    ).a || []
  );
}

/** Spells one argument so that splitArguments() gives it back unchanged. */
export function quoteArgument(argument: string): string {
  const escaped = argument.replace(/[\\"]/g, "\\$&");
  return /[\s"\\]/.test(argument) ? `"${escaped}"` : escaped;
}

const optionOf = (argument: string): string => {
  const eq = argument.indexOf("=");
  return (eq < 0 ? argument : argument.slice(0, eq)).replace(/^-+/, "").toLowerCase();
};

/**
 * Adds the sample to an argument line. A level the line already sets (by
 * either spelling) is left alone: what the user typed wins. Returns the new
 * line and exactly what was added, so that it can be taken out again.
 */
export function addSamples(line: string, sample: SampleThresholds): { line: string; added: string[] } {
  const present = new Set(splitArguments(line).map(optionOf));
  const added: string[] = [];
  if (!present.has("warning") && !present.has("warn")) {
    added.push(...sample.warning.map((expression) => quoteArgument(`warning=${expression}`)));
  }
  if (!present.has("critical") && !present.has("crit")) {
    added.push(...sample.critical.map((expression) => quoteArgument(`critical=${expression}`)));
  }
  const base = line.trimEnd();
  return { line: [base, ...added].filter((part) => part !== "").join(" "), added };
}

/**
 * Takes out what addSamples() added, where it is still there as added. A
 * sample the user has edited since is theirs now, and stays.
 */
export function removeSamples(line: string, added: string[]): string {
  let next = line;
  for (const argument of added) {
    let from = 0;
    for (;;) {
      const at = next.indexOf(argument, from);
      if (at < 0) break;
      const end = at + argument.length;
      // Only a whole argument: not the same text inside a longer one.
      if ((at === 0 || next[at - 1] === " ") && (end === next.length || next[end] === " ")) {
        // With the one space that set it apart, so the rest of the line is
        // left exactly as it was typed.
        const cutFrom = at > 0 ? at - 1 : at;
        const cutTo = at > 0 ? end : Math.min(end + 1, next.length);
        next = next.slice(0, cutFrom) + next.slice(cutTo);
        break;
      }
      from = at + 1;
    }
  }
  return next;
}

const STORAGE_KEY = "nscp.query.simpleMode";

/** Whether simple mode was left on, the last time this browser used the page. */
export function readSimpleMode(): boolean {
  try {
    return window.localStorage.getItem(STORAGE_KEY) === "true";
  } catch {
    // Storage unavailable: the page starts in its ordinary mode.
    return false;
  }
}

export function writeSimpleMode(on: boolean) {
  try {
    window.localStorage.setItem(STORAGE_KEY, on ? "true" : "false");
  } catch {
    // Storage unavailable: the switch still works, it just is not remembered.
  }
}
