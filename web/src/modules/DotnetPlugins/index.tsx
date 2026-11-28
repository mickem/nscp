// .NET plugins (modules/DotnetPlugins): where the plugins and the runtime are,
// and the plugins loaded.
import { fixed, KeyTable, tableKeys } from "../kit/keys.ts";
import ScriptListEditor from "../kit/ScriptListEditor.tsx";
import { simplePage } from "../kit/SimplePage.tsx";
import type { ModulePage } from "../types.ts";

const PATH = "/settings/dotnet";
const TABLE: KeyTable = {
  [PATH]: {
    "plugin path": { builtIn: fixed("${module-path}/dotnet") },
    "runtime path": { builtIn: fixed("") },
    "factory class": { builtIn: fixed("NSCP.Plugin.PluginFactory") },
  },
};

const Plugins = () => (
  <ScriptListEditor
    collectionPath={`${PATH}/plugins`}
    title=".NET plugins"
    subheader="Each assembly is loaded at start; disabled (or 0, false, off, no) skips one."
    noun="plugin"
    fileLabel="Assembly"
    fileHelp="Under the plugin folder unless absolute; .dll may be left off. enabled (or empty) loads the assembly named like the alias."
    emptyMeans={(a) => `${a}.dll`}
  />
);

const DotnetPlugins: ModulePage = {
  id: "DotnetPlugins",
  Panel: simplePage({
    moduleId: "DotnetPlugins",
    title: ".NET plugins",
    subheader: "Loads checks written for .NET.",
    path: PATH,
    table: TABLE,
    section: "Where plugins live",
    fields: [
      {
        key: "plugin path",
        label: "Plugin folder",
        kind: "text",
        help: "Has to hold NSCP.Core.dll and NSCP.Core.runtimeconfig.json; ${exe-path}/modules/dotnet is tried when it does not exist.",
      },
    ],
    advanced: [
      {
        key: "runtime path",
        label: ".NET runtime",
        kind: "text",
        help: "Empty finds it through DOTNET_ROOT, the registered install, or the default folders.",
      },
      {
        key: "factory class",
        label: "Factory class",
        kind: "text",
        help: "The class a plugin assembly creates its plugin with.",
      },
    ],
  }),
  handledKeys: tableKeys(TABLE),
  collections: { [`${PATH}/plugins`]: { kind: "keyValue", Editor: Plugins } },
};

export default DotnetPlugins;
