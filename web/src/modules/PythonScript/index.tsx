// Python scripts (modules/PythonScript): the interpreter, the script root and
// the scripts loaded.
import { fixed, KeyTable, tableKeys } from "../kit/keys.ts";
import ScriptListEditor from "../kit/ScriptListEditor.tsx";
import { simplePage } from "../kit/SimplePage.tsx";
import type { ModulePage } from "../types.ts";

const PATH = "/settings/python";
const TABLE: KeyTable = {
  [PATH]: {
    "script root": { builtIn: fixed(""), inherits: true },
    "python cache": { builtIn: fixed("") },
    // Linux only; the build picks the libpython it was made against.
    "python lib": { builtIn: fixed(""), computedDefault: true },
  },
};

const PythonScripts = () => (
  <ScriptListEditor
    collectionPath={`${PATH}/scripts`}
    title="Python scripts"
    subheader="Each loaded once the interpreter starts."
    noun="script"
    fileLabel="Script file"
    fileHelp="Looked up as given, then under <script root>/python, then the script root; .py may be left off."
  />
);

const PythonScript: ModulePage = {
  id: "PythonScript",
  Panel: simplePage({
    moduleId: "PythonScript",
    title: "Python scripts",
    subheader: "Runs Python scripts that add checks and handle results.",
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
    advanced: [
      {
        key: "python cache",
        label: "Bytecode cache folder",
        kind: "text",
        help: "Where compiled .pyc files go (pycache_prefix); next to the scripts when empty.",
      },
      {
        key: "python lib",
        label: "Python library (Linux)",
        kind: "text",
        help: "The libpython to load, e.g. libpython3.12.so.1.0; the one the agent was built with when empty. Ignored on Windows.",
      },
    ],
  }),
  handledKeys: tableKeys(TABLE),
  collections: { [`${PATH}/scripts`]: { kind: "keyValue", Editor: PythonScripts } },
};

export default PythonScript;
