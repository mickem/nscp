import type { ModulePage } from "../types.ts";
import { tableKeys } from "../kit/keys.ts";
import ExtAliases from "./ExtAliases.tsx";
import ExtScriptsPage from "./ExtScriptsPage.tsx";
import ScriptsEditor from "./ScriptsEditor.tsx";
import WrappedScriptsEditor from "./WrappedScriptsEditor.tsx";
import WrappingsEditor from "./WrappingsEditor.tsx";
import { EXT_ALIAS, EXT_SCRIPTS, EXT_TABLE, EXT_WRAPPED, EXT_WRAPPINGS } from "./ext.ts";

const CheckExternalScripts: ModulePage = {
  id: "CheckExternalScripts",
  Panel: ExtScriptsPage,
  handledKeys: tableKeys(EXT_TABLE),
  collections: {
    [EXT_SCRIPTS]: { kind: "instances", Editor: ScriptsEditor },
    [EXT_ALIAS]: { kind: "instances", Editor: ExtAliases },
    [EXT_WRAPPINGS]: { kind: "keyValue", Editor: WrappingsEditor },
    [EXT_WRAPPED]: { kind: "keyValue", Editor: WrappedScriptsEditor },
  },
};

export default CheckExternalScripts;
