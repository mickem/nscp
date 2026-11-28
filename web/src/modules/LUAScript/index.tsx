// Lua scripts (modules/LUAScript): the script root and the scripts loaded.
import { fixed, KeyTable, tableKeys } from "../kit/keys.ts";
import ScriptListEditor from "../kit/ScriptListEditor.tsx";
import { simplePage } from "../kit/SimplePage.tsx";
import type { ModulePage } from "../types.ts";

const PATH = "/settings/lua";
const TABLE: KeyTable = {
  // include/scripts/script_root_setting.hpp reads it with a parent.
  [PATH]: { "script root": { builtIn: fixed(""), inherits: true } },
};

const LuaScripts = () => (
  <ScriptListEditor
    collectionPath={`${PATH}/scripts`}
    title="Lua scripts"
    subheader="Each loaded at start; a script that fails is reported and the others still load."
    noun="script"
    fileLabel="Script file"
    fileHelp="Looked up as given, then under <script root>/lua, then the script root; .lua may be left off."
  />
);

const LUAScript: ModulePage = {
  id: "LUAScript",
  Panel: simplePage({
    moduleId: "LUAScript",
    title: "Lua scripts",
    subheader: "Runs Lua scripts that add checks and handle results.",
    path: PATH,
    table: TABLE,
    section: "Where scripts live",
    fields: [
      {
        key: "script root",
        label: "Script root",
        kind: "text",
        help: "Empty for ${scripts}, fleet for the scripts a fleet bundle stages, shared, or a folder.",
      },
    ],
  }),
  handledKeys: tableKeys(TABLE),
  collections: { [`${PATH}/scripts`]: { kind: "keyValue", Editor: LuaScripts } },
};

export default LUAScript;
