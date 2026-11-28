import type { ModulePage } from "../types.ts";
import { tableKeys } from "../kit/keys.ts";
import { targetsCollection } from "../kit/targetsCollection.tsx";
import WmiPage from "./WmiPage.tsx";
import WmiQueries from "./WmiQueries.tsx";
import { WMI_QUERIES, WMI_TABLE, WMI_TARGET_SPEC } from "./wmi.ts";

const CheckWMI: ModulePage = {
  id: "CheckWMI",
  Panel: WmiPage,
  handledKeys: tableKeys(WMI_TABLE),
  collections: {
    [WMI_QUERIES]: { kind: "keyValue", Editor: WmiQueries },
    [WMI_TARGET_SPEC.path]: targetsCollection(WMI_TARGET_SPEC),
  },
};

export default CheckWMI;
