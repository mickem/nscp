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
      // way on the tailored page; expert mode shows it.
      isFieldVisible: (key) => (key === "alias" ? false : undefined),
    },
  },
};

export default WEBServer;
