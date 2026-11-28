// What kind of module each one is, for filtering the module list. A module
// can be in more than one group (MSSQL is a check and a database). The agent
// does not say; a module this list does not know (a zip bundle, a script
// module, a .NET plugin) is "Other".

export interface ModuleGroup {
  id: string;
  label: string;
  modules: string[];
}

const DATABASES = ["CheckMSSQL", "CheckMySQL"];
const CONTAINERS = ["CheckDocker", "CheckKubernetes", "CheckHyperV"];

export const MODULE_GROUPS: ModuleGroup[] = [
  {
    id: "checks",
    label: "Checks",
    modules: [
      "CheckSystem",
      "CheckSystemUnix",
      "CheckDisk",
      "CheckEventLog",
      "CheckLogFile",
      "CheckTaskSched",
      "CheckSecurity",
      "CheckWindowsApps",
      "CheckActiveDirectory",
      "CheckNet",
      "CheckWMI",
      "CheckNSCP",
      "CheckHelpers",
      ...DATABASES,
      ...CONTAINERS,
    ],
  },
  { id: "databases", label: "Databases", modules: DATABASES },
  { id: "containers", label: "Containers & VMs", modules: CONTAINERS },
  {
    id: "servers",
    label: "Servers",
    modules: [
      "NRPEServer",
      "NSCAServer",
      "NSClientServer",
      "NCPAServer",
      "CheckMKServer",
      "WEBServer",
      "IcingaAgent",
    ],
  },
  {
    id: "clients",
    label: "Clients",
    modules: [
      "NRPEClient",
      "NSCAClient",
      "NSCANgClient",
      "NRDPClient",
      "NSCPClient",
      "SMTPClient",
      "SyslogClient",
      "GraphiteClient",
      "CollectdClient",
      "CheckMKClient",
      "GearmanClient",
      "IcingaClient",
      "Op5Client",
      "ElasticClient",
    ],
  },
  {
    id: "scripts",
    label: "Scripts",
    modules: [
      "CheckExternalScripts",
      "CheckPowershell",
      "LUAScript",
      "PythonScript",
      "DotnetPlugins",
    ],
  },
  {
    id: "scheduling",
    label: "Scheduling & results",
    modules: ["Scheduler", "SimpleCache", "SimpleFileWriter"],
  },
];

export const OTHER_GROUP = { id: "other", label: "Other" };

const KNOWN = new Set(MODULE_GROUPS.flatMap((g) => g.modules));

// The groups a module is in, by id; "other" for one no group names.
export function groupsOf(moduleId: string): string[] {
  const ids = MODULE_GROUPS.filter((g) => g.modules.includes(moduleId)).map((g) => g.id);
  return ids.length > 0 ? ids : [OTHER_GROUP.id];
}

export function isKnownModule(moduleId: string): boolean {
  return KNOWN.has(moduleId);
}
