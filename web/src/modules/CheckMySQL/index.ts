import type { ModulePage } from "../types.ts";
import { tableKeys } from "../kit/keys.ts";
import MysqlPage from "./MysqlPage.tsx";
import { MYSQL_TABLE } from "./mysql.ts";

const CheckMySQL: ModulePage = {
  id: "CheckMySQL",
  Panel: MysqlPage,
  handledKeys: tableKeys(MYSQL_TABLE),
};

export default CheckMySQL;
