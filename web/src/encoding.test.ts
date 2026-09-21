import { describe, expect, it } from "vitest";
import { readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";

/**
 * Guards the source tree against text that was decoded as Windows-1252 and
 * re-encoded as UTF-8 - "mojibake". An em dash written by an editor that saved
 * UTF-8 and reopened it as the ANSI codepage comes back as three characters,
 * and the file is still perfectly valid UTF-8 afterwards, so nothing downstream
 * complains: the bundle builds, the page renders, and the metrics filter box
 * shows its placeholder with three stray characters on the end where the
 * ellipsis should be.
 *
 * The live case was six files under src/pages carrying twelve of these, seven
 * of them in strings a user reads. This file deliberately contains no example
 * of the damage, because it scans itself along with everything else.
 */
// vitest runs with the web/ package root as the working directory.
const SRC = join(process.cwd(), "src");
const EXTS = [".ts", ".tsx", ".css", ".html"];

function sources(dir: string): string[] {
  return readdirSync(dir, { withFileTypes: true }).flatMap((entry) => {
    const full = join(dir, entry.name);
    if (entry.isDirectory()) return sources(full);
    return EXTS.some((e) => entry.name.endsWith(e)) ? [full] : [];
  });
}

/**
 * The round trip that only mojibake survives: re-encode the line in cp1252 and
 * read the result back as UTF-8. Text that was never mangled either fails to
 * encode (no cp1252 byte for it), fails to decode (the bytes are not a UTF-8
 * sequence), or comes back unchanged. Text that was mangled comes back as what
 * it should have said.
 */
function repaired(line: string): string | undefined {
  const cp1252 = new Map<number, number>();
  // The 27 printable code points cp1252 puts in 0x80-0x9F, which is the whole
  // difference between it and Latin-1 and the only reason mojibake is lossless.
  const high = "€‚ƒ„…†‡ˆ‰Š‹Œ"
    + "Ž‘’“”•–—˜™š›œ"
    + "žŸ";
  for (let i = 0; i < high.length; i++) cp1252.set(high.charCodeAt(i), 0x80 + i);

  const bytes: number[] = [];
  for (const ch of line) {
    const code = ch.codePointAt(0)!;
    const mapped = cp1252.get(code);
    if (mapped !== undefined) bytes.push(mapped);
    else if (code <= 0xff) bytes.push(code);
    else return undefined; // no cp1252 byte for it - never was mojibake
  }
  let decoded: string;
  try {
    decoded = new TextDecoder("utf-8", { fatal: true }).decode(new Uint8Array(bytes));
  } catch {
    return undefined; // not a UTF-8 sequence - ordinary Latin-1 text
  }
  return decoded === line ? undefined : decoded;
}

describe("source encoding", () => {
  it("has no text that was round-tripped through Windows-1252", () => {
    const damage: string[] = [];
    for (const file of sources(SRC)) {
      const lines = readFileSync(file, "utf8").split("\n");
      lines.forEach((line, i) => {
        const fixed = repaired(line);
        if (fixed !== undefined) {
          damage.push(`${file}:${i + 1}\n  is: ${line.trim()}\n  sb: ${fixed.trim()}`);
        }
      });
    }
    expect(damage).toEqual([]);
  });
});
