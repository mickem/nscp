// The interactive prompt of nscp test (modules/CommandClient).
import { fixed, KeyTable, tableKeys } from "../kit/keys.ts";
import { simplePage } from "../kit/SimplePage.tsx";
import type { ModulePage } from "../types.ts";

const PATH = "/settings/cli";
const TABLE: KeyTable = {
  [PATH]: {
    "history size": { builtIn: fixed("500") },
    "history file": { builtIn: fixed(""), computedDefault: true },
    color: { builtIn: fixed("true") },
  },
};

const CommandClient: ModulePage = {
  id: "CommandClient",
  Panel: simplePage({
    moduleId: "CommandClient",
    title: "Command line",
    subheader: "The interactive prompt of nscp test.",
    path: PATH,
    table: TABLE,
    section: "Prompt",
    fields: [
      {
        key: "history size",
        label: "Commands kept in the history",
        kind: "number",
        help: "0 keeps no history on disk: what you want when typed commands may carry credentials.",
      },
      {
        key: "history file",
        label: "History file",
        kind: "text",
        help: "The per-user default when empty: %APPDATA%\\NSClient++\\console-history.txt, or ~/.nscp_history.",
      },
      {
        key: "color",
        label: "Colour",
        kind: "bool",
        help: "Colour the prompt, what is typed and log messages by severity.",
      },
    ],
  }),
  handledKeys: tableKeys(TABLE),
};

export default CommandClient;
