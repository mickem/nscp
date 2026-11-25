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
import CheckNSCP from "./CheckNSCP/index.ts";
import CheckSystem from "./CheckSystem/index.ts";
import CheckSystemUnix from "./CheckSystemUnix/index.ts";
import IcingaClient from "./IcingaClient/index.ts";
import NRPEServer from "./NRPEServer/index.ts";
import WEBServer from "./WEBServer/index.ts";

export const MODULE_PAGES: ReadonlyArray<ModulePage> = [
  CheckEventLog,
  CheckHelpers,
  CheckLogFile,
  CheckNSCP,
  CheckSystem,
  CheckSystemUnix,
  IcingaClient,
  NRPEServer,
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
