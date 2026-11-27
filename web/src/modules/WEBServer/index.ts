import type { ModulePage } from "../types.ts";
import { tableKeys } from "../kit/keys.ts";
import RolesEditor from "./RolesEditor.tsx";
import WebPage from "./WebPage.tsx";
import { WEB_TABLE } from "./web.ts";

export const WEB_ROLES_PATH = "/settings/WEB/server/roles";
const USERS = "/settings/WEB/server/users";

const WEBServer: ModulePage = {
  id: "WEBServer",
  Panel: WebPage,
  handledKeys: tableKeys(WEB_TABLE),
  purpose: {
    tools: ["Icinga", "REST clients"],
    transport: "REST",
    direction: "active",
    platforms: ["windows", "linux", "macos"],
  },
  collections: {
    [WEB_ROLES_PATH]: { kind: "keyValue", Editor: RolesEditor },
    [USERS]: {
      kind: "instances",
      fields: { role: { kind: "role" } },
      // A user is a password and a role: too little for templates to pay off.
      templates: false,
      // `alias` is an advanced authentication mechanism — keep it out of the
      // way on the tailored page; expert mode shows it.
      isFieldVisible: (key) => (key === "alias" ? false : undefined),
    },
  },
};

export default WEBServer;
