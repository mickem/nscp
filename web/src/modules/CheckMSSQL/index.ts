import type { ModulePage } from "../types.ts";
import { tableKeys } from "../kit/keys.ts";
import MssqlPage from "./MssqlPage.tsx";
import { MSSQL_TABLE } from "./mssql.ts";

const CheckMSSQL: ModulePage = {
  id: "CheckMSSQL",
  Panel: MssqlPage,
  handledKeys: tableKeys(MSSQL_TABLE),
};

export default CheckMSSQL;
