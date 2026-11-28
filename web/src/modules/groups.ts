// The groups of the module list, from each module's category (module.json
// "category", reported as metadata.category by /api/v2/modules). A database
// or a container check is a check too, so those sit in two groups. A module
// without a category (a zip bundle that declares none, ...) is Other.

// Keep in step with build/python/create_plugin_module.py (MODULE_CATEGORIES)
// and docs/hooks/reference_nav.py.
export const MODULE_CATEGORIES = [
  "check",
  "database",
  "container",
  "server",
  "client",
  "script",
  "scheduling",
  "tool",
] as const;

export interface ModuleGroup {
  id: string;
  label: string;
  categories: string[];
}

export const MODULE_GROUPS: ModuleGroup[] = [
  { id: "checks", label: "Checks", categories: ["check", "database", "container"] },
  { id: "databases", label: "Databases", categories: ["database"] },
  { id: "containers", label: "Containers & VMs", categories: ["container"] },
  { id: "servers", label: "Servers", categories: ["server"] },
  { id: "clients", label: "Clients", categories: ["client"] },
  { id: "scripts", label: "Scripts", categories: ["script"] },
  { id: "scheduling", label: "Scheduling & results", categories: ["scheduling"] },
];

export const OTHER_GROUP = { id: "other", label: "Other" };

// The groups a module of this category is in; Other for a tool, or for one
// that declares no category the list knows.
export function groupsOf(category: string | undefined): string[] {
  const ids = MODULE_GROUPS.filter((g) => g.categories.includes(category ?? "")).map((g) => g.id);
  return ids.length > 0 ? ids : [OTHER_GROUP.id];
}
