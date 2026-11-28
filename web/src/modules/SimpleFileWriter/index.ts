// The file writer (modules/SimpleFileWriter): results submitted on a
// channel, appended to a file one line each.
import { fixed, KeyTable, tableKeys } from "../kit/keys.ts";
import { simplePage } from "../kit/SimplePage.tsx";
import type { ModulePage } from "../types.ts";

const PATH = "/settings/writers/file";
const TABLE: KeyTable = {
  [PATH]: {
    channel: { builtIn: fixed("FILE") },
    file: { builtIn: fixed("output.txt") },
    syntax: { builtIn: fixed("${alias-or-command} ${result} ${message}") },
    "service-syntax": { builtIn: fixed("") },
    "host-syntax": { builtIn: fixed("") },
    "time-syntax": { builtIn: fixed("%Y-%m-%d %H:%M:%S") },
  },
};

const KEYWORDS =
  "${alias-or-command}, ${command}, ${alias}, ${host}, ${channel}, ${result}, ${message}, ${epoch}, ${time}.";

const SimpleFileWriter: ModulePage = {
  id: "SimpleFileWriter",
  Panel: simplePage({
    moduleId: "SimpleFileWriter",
    title: "File writer",
    subheader: "Appends every result submitted on a channel to a file, one line each.",
    path: PATH,
    table: TABLE,
    section: "Where results go",
    fields: [
      {
        key: "channel",
        label: "Channel",
        kind: "text",
        help: "Results submitted here are written: point a scheduler or a real-time check at it.",
      },
      {
        key: "file",
        label: "File",
        kind: "text",
        help: "A bare name is under the log folder; an absolute path is used as given.",
      },
      { key: "syntax", label: "Line", kind: "text", help: `Any text with ${KEYWORDS}` },
    ],
    advanced: [
      {
        key: "service-syntax",
        label: "Line for a service result",
        kind: "text",
        help: "In place of Line for service results; Line when empty.",
      },
      {
        key: "host-syntax",
        label: "Line for a host result",
        kind: "text",
        help: "In place of Line for host results; Line when empty.",
      },
      {
        key: "time-syntax",
        label: "Format of ${time}",
        kind: "text",
        help: "strftime flags; the time the line is written: %Y-%m-%d %H:%M:%S",
      },
    ],
  }),
  handledKeys: tableKeys(TABLE),
};

export default SimpleFileWriter;
