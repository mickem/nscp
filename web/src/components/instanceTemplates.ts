// Pre-fill recipes for the "Add" button on a SettingsCollection. Picking a
// template materializes a new instance with every field already populated, so
// the user only has to confirm/tweak the name.
//
// To add a new recipe: drop another entry into the TEMPLATES array. Each
// entry's `pathPattern` is matched against the collection root path; the
// `templates` list is what shows in the Add dropdown.

export interface InstanceTemplate {
  // Display name in the Add menu.
  name: string;
  // Optional sub-line in the menu and dialog hint.
  description?: string;
  // Suggested name for the new instance — pre-filled in the create dialog.
  defaultName: string;
  // Schema-key → value pairs to write under the new path. Empty values are
  // skipped (the backend interprets "" as a delete).
  fields: Record<string, string>;
}

interface TemplateRegistration {
  pathPattern: RegExp;
  templates: InstanceTemplate[];
}

const TEMPLATES: TemplateRegistration[] = [
  {
    pathPattern: /\/system\/windows\/real-time\/cpu$/,
    templates: [
      {
        name: "Cpu above 80%",
        description: "Raise an event when CPU usage stays above 80% for a minute.",
        defaultName: "cpu_above_80",
        fields: {
          time: "60",
          filter: "total > 80%",
          warning: "count > 0",
          "top syntax": "%(list)",
          "detail syntax": "%(core): system: %(system)%, user: %(user)",
          "silent period": "5m",
          destination: "event",
        },
      },
    ],
  },
  {
    pathPattern: /\/system\/windows\/real-time\/memory$/,
    templates: [
      {
        name: "Memory above 50%",
        description: "Raise an event when physical memory usage exceeds 50%.",
        defaultName: "memory_above_50",
        fields: {
          type: "physical",
          filter: "used > 50%",
          warning: "count > 0",
          "top syntax": "%(list)",
          "detail syntax": "%(type): %(used_pct)%",
          "silent period": "5m",
          destination: "event",
        },
      },
    ],
  },
  {
    pathPattern: /\/system\/windows\/real-time\/process$/,
    templates: [
      {
        name: "Any process has started",
        description: "Alert when a new instance of any process appears.",
        defaultName: "process_started",
        fields: {
          process: "*",
          filter: "new = 1",
          warning: "count>0",
          severity: "ok",
          "top syntax": "%(list)",
          "detail syntax": "%(exe)",
          destination: "event",
        },
      },
      {
        name: "Process has stopped",
        description: "Alert when a named process is no longer running.",
        defaultName: "process_stopped",
        fields: {
          process: "CalculatorApp",
          filter: "state=stopped",
          critical: "count==0",
          severity: "critical",
          "top syntax": "%(list)",
          "detail syntax": "%(exe)",
          destination: "event",
        },
      },
    ],
  },
];

export function getFactoryTemplates(collectionPath: string): InstanceTemplate[] {
  for (const reg of TEMPLATES) {
    if (reg.pathPattern.test(collectionPath)) return reg.templates;
  }
  return [];
}
