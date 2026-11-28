// An alias or schedule's command line: a command name, then its arguments as
// the agent splits them (str::utils::parse_command, boost's
// escaped_list_separator): on spaces, with "double quotes" keeping a token
// together and \\, \" , \<space> and \n as the only escapes. Single quotes are
// plain characters. Empty tokens are dropped.

export class CommandLineError extends Error {}

export function tokenize(line: string): string[] {
  const out: string[] = [];
  let cur = "";
  let quoted = false;
  for (let i = 0; i < line.length; ++i) {
    const ch = line[i];
    if (ch === "\\") {
      const next = line[i + 1];
      if (next === "\\" || next === '"' || next === " ") cur += next;
      else if (next === "n") cur += "\n";
      else
        throw new CommandLineError(
          "Only \\\\, \\\" and \\<space> are escapes: write a Windows path's backslash as \\\\ or use /.",
        );
      ++i;
    } else if (ch === '"') quoted = !quoted;
    else if (ch === " " && !quoted) {
      out.push(cur);
      cur = "";
    } else cur += ch;
  }
  out.push(cur);
  return out.filter((t) => t !== "");
}

// Why the agent would refuse the line, or null.
export function commandLineProblem(line: string): string | null {
  try {
    const tokens = tokenize(line);
    if (tokens.some((t) => /[\r\n]/.test(t)))
      return "A line break is not allowed: write \\n as \\\\n, and a Windows path's backslash as \\\\.";
    return null;
  } catch (e) {
    return e instanceof Error ? e.message : String(e);
  }
}

// The command and the rest of the line, kept as typed: the arguments are
// not trimmed at the end, so a space typed after the last one stays.
export function splitCommand(line: string): { command: string; args: string } {
  const start = line.replace(/^\s+/, "");
  const at = start.search(/\s/);
  return at < 0
    ? { command: start, args: "" }
    : { command: start.slice(0, at), args: start.slice(at).replace(/^\s+/, "") };
}

export function joinCommand(command: string, args: string): string {
  return args !== "" ? `${command.trim()} ${args}` : command.trim();
}

// A token as it has to be written to come back as itself.
export function quote(token: string): string {
  const escaped = token.replace(/\\/g, "\\\\").replace(/"/g, '\\"');
  return /[\s"]/.test(token) ? `"${escaped}"` : escaped;
}

// The $ARGn$ / $ARGS$ placeholders a line takes from its caller.
export function placeholders(line: string): string[] {
  return [...new Set(line.match(/\$ARG(?:\d+|S"?)\$|%ARG\d+%/g) ?? [])];
}
