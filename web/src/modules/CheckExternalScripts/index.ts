import type { ModulePage } from "../types.ts";
import { tableKeys } from "../kit/keys.ts";
import ExtAliases from "./ExtAliases.tsx";
import ExtScriptsPage from "./ExtScriptsPage.tsx";
import ScriptsEditor from "./ScriptsEditor.tsx";
import { EXT_ALIAS, EXT_SCRIPTS, EXT_TABLE, EXT_WRAPPED, EXT_WRAPPINGS } from "./ext.ts";

const CheckExternalScripts: ModulePage = {
  id: "CheckExternalScripts",
  Panel: ExtScriptsPage,
  handledKeys: tableKeys(EXT_TABLE),
  collections: {
    [EXT_SCRIPTS]: { kind: "instances", Editor: ScriptsEditor },
    [EXT_ALIAS]: { kind: "instances", Editor: ExtAliases },
    // ps1, vbs and bat have built-in wrappings; an extension of your own goes here.
    [EXT_WRAPPINGS]: {
      kind: "keyValue",
      presets: [
        {
          label: "py",
          description: "Python scripts, run with python from the PATH.",
          defaultKey: "py",
          defaultValue: 'python "scripts\\%SCRIPT%" $ARGS"$',
        },
        {
          label: "sh",
          description: "Shell scripts (Linux).",
          defaultKey: "sh",
          defaultValue: '/bin/sh "scripts/%SCRIPT%" $ARGS"$',
        },
      ],
    },
    [EXT_WRAPPED]: { kind: "keyValue" },
  },
};

export default CheckExternalScripts;
