import type { ModulePage } from "../types.ts";

export const WEB_ROLES_PATH = "/settings/WEB/server/roles";
const USERS = "/settings/WEB/server/users";

const WEBServer: ModulePage = {
  id: "WEBServer",
  collections: {
    [WEB_ROLES_PATH]: { kind: "keyValue" },
    [USERS]: {
      kind: "instances",
      fields: { role: { kind: "role" } },
      // `alias` is an advanced authentication mechanism — keep it out of the
      // way unless the user has opted into advanced fields.
      isFieldVisible: (key, { showAdvanced }) =>
        key === "alias" && !showAdvanced ? false : undefined,
    },
  },
};

export default WEBServer;
