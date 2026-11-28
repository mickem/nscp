import type { ModulePage } from "../types.ts";
import { tableKeys } from "../kit/keys.ts";
import DiskFiles from "./DiskFiles.tsx";
import DiskPage from "./DiskPage.tsx";
import { DISK_FILES, DISK_TABLE } from "./disk.ts";

const CheckDisk: ModulePage = {
  id: "CheckDisk",
  Panel: DiskPage,
  handledKeys: tableKeys(DISK_TABLE),
  collections: { [DISK_FILES]: { kind: "keyValue", Editor: DiskFiles } },
};

export default CheckDisk;
