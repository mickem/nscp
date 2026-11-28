// Hyper-V (modules/CheckHyperV): the checks take no settings; the VM
// inventory is a fact set switched on here.
import { fixed, KeyTable, tableKeys } from "../kit/keys.ts";
import { simplePage } from "../kit/SimplePage.tsx";
import type { ModulePage } from "../types.ts";

const PATH = "/settings/hyperv/facts";
const TABLE: KeyTable = { [PATH]: { "hyperv.vms": { builtIn: fixed("false") } } };

const CheckHyperV: ModulePage = {
  id: "CheckHyperV",
  Panel: simplePage({
    moduleId: "CheckHyperV",
    title: "Hyper-V",
    subheader: "Checks the virtual machines and the Hyper-V host (Windows).",
    path: PATH,
    table: TABLE,
    section: "Facts",
    fields: [
      {
        key: "hyperv.vms",
        label: "Report the virtual machines",
        kind: "bool",
        help: "Name, GUID, generation, processors, memory, checkpoints and replica role of each VM, as hyperv.vms. Not state or load.",
      },
    ],
  }),
  handledKeys: tableKeys(TABLE),
};

export default CheckHyperV;
