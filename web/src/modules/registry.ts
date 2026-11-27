// The module pages, registered by module id.
//
// To give a module a tailored page, add a folder `web/src/modules/<ModuleId>/`
// whose index exports a `ModulePage`, and list it here. A module without an
// entry gets the generic schema-driven settings only.
import type {
  CollectionConfig,
  FieldWidget,
  InstanceCollectionConfig,
  KeyValueCollectionConfig,
  ModulePage,
} from "./types.ts";
import CheckEventLog from "./CheckEventLog/index.ts";
import CheckHelpers from "./CheckHelpers/index.ts";
import CheckLogFile from "./CheckLogFile/index.ts";
import CheckMKServer from "./CheckMKServer/index.ts";
import CheckNSCP from "./CheckNSCP/index.ts";
import CheckSystem from "./CheckSystem/index.ts";
import CheckSystemUnix from "./CheckSystemUnix/index.ts";
import IcingaAgent from "./IcingaAgent/index.ts";
import IcingaClient from "./IcingaClient/index.ts";
import NCPAServer from "./NCPAServer/index.ts";
import NRPEServer from "./NRPEServer/index.ts";
import NSCAServer from "./NSCAServer/index.ts";
import NSClientServer from "./NSClientServer/index.ts";
import WEBServer from "./WEBServer/index.ts";
import CheckMKClient from "./CheckMKClient/index.ts";
import CollectdClient from "./CollectdClient/index.ts";
import GearmanClient from "./GearmanClient/index.ts";
import GraphiteClient from "./GraphiteClient/index.ts";
import NRDPClient from "./NRDPClient/index.ts";
import NRPEClient from "./NRPEClient/index.ts";
import NSCAClient from "./NSCAClient/index.ts";
import NSCANgClient from "./NSCANgClient/index.ts";
import NSCPClient from "./NSCPClient/index.ts";
import SMTPClient from "./SMTPClient/index.ts";
import SyslogClient from "./SyslogClient/index.ts";

export const MODULE_PAGES: ReadonlyArray<ModulePage> = [
  CheckEventLog,
  CheckHelpers,
  CheckLogFile,
  CheckMKClient,
  CheckMKServer,
  CheckNSCP,
  CheckSystem,
  CheckSystemUnix,
  CollectdClient,
  GearmanClient,
  GraphiteClient,
  IcingaAgent,
  IcingaClient,
  NCPAServer,
  NRDPClient,
  NRPEClient,
  NRPEServer,
  NSCAClient,
  NSCANgClient,
  NSCAServer,
  NSClientServer,
  NSCPClient,
  SMTPClient,
  SyslogClient,
  WEBServer,
];

const BY_ID = new Map<string, ModulePage>(MODULE_PAGES.map((p) => [p.id, p]));

// Every collection and section of every page, by exact path. Paths are owned
// by exactly one module, which `registry.test.ts` holds to.
const COLLECTIONS = new Map<string, CollectionConfig>();
const SECTIONS = new Map<string, Record<string, FieldWidget>>();
for (const page of MODULE_PAGES) {
  for (const [path, config] of Object.entries(page.collections ?? {}))
    COLLECTIONS.set(path, config);
  for (const [path, widgets] of Object.entries(page.sections ?? {})) SECTIONS.set(path, widgets);
}

export function getModulePage(moduleId: string): ModulePage | undefined {
  return BY_ID.get(moduleId);
}

export function getCollectionConfig(path: string): CollectionConfig | undefined {
  return COLLECTIONS.get(path);
}

export function getInstanceCollectionConfig(path: string): InstanceCollectionConfig | undefined {
  const config = COLLECTIONS.get(path);
  return config?.kind === "instances" ? config : undefined;
}

export function getKeyValueCollectionConfig(path: string): KeyValueCollectionConfig | undefined {
  const config = COLLECTIONS.get(path);
  return config?.kind === "keyValue" ? config : undefined;
}

function parentPath(path: string): string {
  const i = path.lastIndexOf("/");
  return i > 0 ? path.slice(0, i) : "";
}

// The widget a module page declares for `key` at `path`: either on the
// section itself, or on the collection `path` is an instance of.
export function findFieldWidget(path: string, key: string): FieldWidget | undefined {
  const own = SECTIONS.get(path)?.[key];
  if (own) return own;
  return getInstanceCollectionConfig(parentPath(path))?.fields?.[key];
}
