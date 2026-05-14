import { createApi, fetchBaseQuery, FetchBaseQueryMeta } from "@reduxjs/toolkit/query/react";
import { BaseQueryFn, FetchArgs, FetchBaseQueryError } from "@reduxjs/toolkit/query";
import { RootState } from "../store/store.ts";
import { authSlice } from "../common/authSlice.ts";

type EndpointList = { [key: string]: string };

export interface Page<Type> {
  content: Type;
  page: number;
  pages: number;
  limit: number;
  count: number;
}

export interface PaginatedQuery {
  page?: number;
  size?: number;
}

export interface LogQuery extends PaginatedQuery {
  level?: string;
}

export type Metrics = { [key: string]: string | number };
export type ApiErrorResponse = { error: string; status: number };

export const responseHandler = (response: Response): Promise<ApiErrorResponse | unknown> => {
  if (
    response.status === 400 ||
    response.status === 401 ||
    response.status === 403 ||
    response.status === 404 ||
    response.status === 500
  ) {
    return new Promise((resolve) => {
      response.text().then((text) => {
        resolve({ error: text, status: response.status });
      });
    });
  }
  return Promise.resolve(response.json());
};

export function transformPaginatedResponse<T>(apiResponse: T, meta: FetchBaseQueryMeta) {
  const count = +(meta?.response?.headers.get("X-Pagination-Count") || 0);
  const page = +(meta?.response?.headers.get("X-Pagination-Page") || 0);
  const limit = +(meta?.response?.headers.get("X-Pagination-Limit") || 0);
  const pages = Math.ceil(count / limit);
  return {
    content: apiResponse,
    page,
    limit,
    count,
    pages,
  } as Page<T>;
}

const encodeArgs = (args: string[]) => {
  return args
    .map((value) => {
      if (value === "") {
        return "";
      }
      if (value.includes("=")) {
        return value
          .split("=")
          .map((v) => `${encodeURIComponent(v)}`)
          .join("=");
      }
      return `${encodeURIComponent(value)}`;
    })
    .join("&");
};

interface Info {
  name: string;
  version: string;
  version_url: string;
}

interface Version {
  version: string;
}

export interface LogRecord {
  date: string;
  file: string;
  level: "critical" | "error" | "warning" | "info" | "success" | "debug" | "unknown";
  line: number;
  message: string;
}

export interface LogStatus {
  errors: number;
  last_error: string;
}

interface ModulesQuery {
  all: boolean;
}

export interface ModuleListItem {
  id: string;
  name: string;
  title: string;
  description: string;
  enabled: boolean;
  loaded: boolean;
  metadata: {
    alias: string;
    plugin_id: string;
  };
  load_url: string;
  unload_url: string;
  module_url: string;
}

interface Module {
  id: string;
  name: string;
  title: string;
  enabled: boolean;
  description: string;
  loaded: boolean;
  metadata: {
    alias: string;
    plugin_id: string;
  };
  load_url: string;
  unload_url: string;
  disable_url: string;
  enable_url: string;
}

interface QueryListItem {
  name: string;
  title: string;
  plugin: string;
  description: string;
  query_url: string;
}

export interface AliasListItem {
  name: string;
  title: string;
  plugin: string;
  description: string;
  alias_url: string;
}

interface Query {
  name: string;
  title: string;
  plugin: string;
  description: string;
  execute_nagios_url: string;
  execute_url: string;
}

export interface ExecuteQueryArgs {
  query: string;
  args: string[];
}

export interface QueryExecutionResultLinePerf {
  // `warning` / `critical` are JSON numbers for plain-numeric thresholds
  // (the historical case) and JSON strings when the upstream plugin used
  // Nagios range syntax (e.g. "4:5", ":10", "@0:90", "~:10"). See GitHub
  // issue #748 - the backend used to silently truncate ranges to their
  // numeric lower bound; consumers that did arithmetic on these fields
  // now need to guard against the string case (typeof === "number").
  critical: number | string;
  maximum: number;
  minimum: number;
  unit: string;
  value: number;
  warning: number | string;
}

export interface QueryExecutionResultLine {
  message: string;
  perf: {
    [key: string]: QueryExecutionResultLinePerf;
  };
}

export interface QueryExecutionResult {
  command: string;
  result: 0 | 1 | 2 | 3;
  lines: QueryExecutionResultLine[];
}

interface Script {}

export interface SettingsStatus {
  context: string;
  type: string;
  has_changed: boolean;
}

export interface Settings {
  key: string;
  path: string;
  value: string;
}

export interface SettingsCommand {
  command: "load" | "save" | "reload";
}

export type SettingsDiffChangeType =
  | "modified"
  | "added"
  | "removed"
  | "path_added"
  | "path_removed";

export interface SettingsDiffEntry {
  path: string;
  key: string;
  old_value: string;
  new_value: string;
  change_type: SettingsDiffChangeType;
  is_sensitive: boolean;
}

export interface SettingsDiff {
  entries: SettingsDiffEntry[];
  count: number;
}

// Normalized shape for metadata autocomplete endpoints (counters, channels…).
// `value` is what's persisted; `label` is what the dropdown displays.
export interface MetadataOption {
  value: string;
  label: string;
}

export interface EventEntry {
  index: number;
  event: string;
  date: string;
  data: { [key: string]: string };
}

export interface SettingsDescription {
  default_value: string;
  description: string;
  icon: string;
  is_advanced_key: boolean;
  is_object: boolean;
  is_sample_key: boolean;
  is_template_key: boolean;
  key: string;
  path: string;
  type: string;
  plugins: string[];
  sample_usage: string;
  title: string;
  value: string;
}

const baseQuery = fetchBaseQuery({
  baseUrl: "/api",
  prepareHeaders: (headers, { getState }) => {
    if (!headers.has("authorization")) {
      const token = (getState() as RootState).auth.token;
      headers.set("authorization", `Bearer ${token}`);
    }
    if (!headers.has("Content-Type")) {
      headers.set("Content-Type", "application/json");
    }
    return headers;
  },
  validateStatus: (response) => {
    return response.status >= 200 && response.status <= 299;
  },
  responseHandler: (response: Response): Promise<ApiErrorResponse | unknown> => {
    if (response.status === 400 || response.status === 401 || response.status === 403 || response.status === 404) {
      return new Promise((resolve) => {
        response.text().then((text) => {
          resolve({ error: text, status: response.status });
        });
      });
    }
    return Promise.resolve(response.json());
  },
});

const baseQueryWithAuthFail: BaseQueryFn<string | FetchArgs, unknown, FetchBaseQueryError> = async (
  args,
  api,
  extraOptions,
) => {
  const result = await baseQuery(args, api, extraOptions);
  if (result.error && result.error.status === 403) {
    api.dispatch(authSlice.actions.removeToken());
    // Drop every cached response from the previous session so a re-login
    // doesn't show stale data while the new requests are in flight.
    api.dispatch(nsclientApi.util.resetApiState());
  }
  return result;
};

// Every cache tag in the API. Mutations that touch state across the whole
// service (module load/unload, etc.) invalidate this list so all queries
// refetch instead of having to enumerate the affected tags by hand.
export const ALL_API_TAGS = [
  "Endpoints",
  "Info",
  "Version",
  "Logs",
  "Modules",
  "Module",
  "Query",
  "Queries",
  "Aliases",
  "Scripts",
  "Settings",
  "SettingsStatus",
  "SettingsDescriptions",
  "LogStatus",
  "Metrics",
  "Events",
] as const;

export const nsclientApi = createApi({
  reducerPath: "api",
  baseQuery: baseQueryWithAuthFail,
  tagTypes: [
    "Endpoints",
    "Info",
    "Version",
    "Logs",
    "Modules",
    "Module",
    "Query",
    "Queries",
    "Aliases",
    "Scripts",
    "Settings",
    "SettingsStatus",
    "SettingsDescriptions",
    "LogStatus",
    "Metrics",
    "Events",
  ],
  endpoints: (builder) => ({
    getEndpoints: builder.query<EndpointList, void>({
      query: () => ({
        url: "/v2",
      }),
      providesTags: ["Endpoints"],
    }),
    getInfo: builder.query<Info, void>({
      query: () => ({
        url: "/v2/info",
      }),
      providesTags: ["Info"],
    }),
    getVersion: builder.query<Version, void>({
      query: () => ({
        url: "/v2/info/version",
      }),
      providesTags: ["Version"],
    }),
    getLogs: builder.query<Page<LogRecord[]>, LogQuery>({
      query: ({ page, size, level }) => ({
        url: `/v2/logs?page=${page || 0}&size=${size || 10}${level ? `&level=${level}` : ""}`,
        responseHandler,
      }),
      providesTags: ["Logs"],
      transformResponse: transformPaginatedResponse<LogRecord[]>,
    }),
    getLogStatus: builder.query<LogStatus, void>({
      query: () => ({
        url: "/v2/logs/status",
        responseHandler,
      }),
      providesTags: ["LogStatus"],
    }),
    resetLogStatus: builder.mutation<void, void>({
      query: () => ({
        url: "/v2/logs/status",
        method: "DELETE",
      }),
      invalidatesTags: ["LogStatus", "Logs"],
    }),
    getModules: builder.query<ModuleListItem[], ModulesQuery>({
      query: ({ all }) => ({
        url: `/v2/modules?all=${all ? "true" : "false"}`,
      }),
      providesTags: ["Modules"],
    }),
    getModule: builder.query<Module, string>({
      query: (id) => ({
        url: `/v2/modules/${id}`,
      }),
      providesTags: (_result, _error, id) => [{ type: "Module", id }],
    }),
    loadModule: builder.mutation<string, string>({
      query: (id) => ({
        url: `/v2/modules/${id}/commands/load`,
        method: "GET",
      }),
      // Loading a module exposes new endpoints, queries, settings, metrics,
      // etc. — refetch everything rather than listing the affected tags.
      invalidatesTags: () => [...ALL_API_TAGS],
    }),
    unloadModule: builder.mutation<string, string>({
      query: (id) => ({
        url: `/v2/modules/${id}/commands/unload`,
        method: "GET",
      }),
      // Unloading similarly removes endpoints/queries/settings — full refetch.
      invalidatesTags: () => [...ALL_API_TAGS],
    }),
    enableModule: builder.mutation<string, string>({
      query: (id) => ({
        url: `/v2/modules/${id}/commands/enable`,
        method: "GET",
      }),
      invalidatesTags: (_result, _error, id) => [{ type: "Module", id }, { type: "Modules" }],
    }),
    disableModule: builder.mutation<string, string>({
      query: (id) => ({
        url: `/v2/modules/${id}/commands/disable`,
        method: "GET",
      }),
      invalidatesTags: (_result, _error, id) => [{ type: "Module", id }, { type: "Modules" }],
    }),
    getQueries: builder.query<QueryListItem[], void>({
      query: () => ({
        url: "/v2/queries",
      }),
      providesTags: ["Queries"],
    }),
    getAliases: builder.query<AliasListItem[], void>({
      query: () => ({
        url: "/v2/aliases",
      }),
      providesTags: ["Aliases"],
    }),
    getQuery: builder.query<Query, string>({
      query: (id) => ({
        url: `/v2/queries/${id}`,
      }),
      providesTags: (_result, _error, id) => [{ type: "Query", id }],
    }),
    executeQuery: builder.mutation<QueryExecutionResult, ExecuteQueryArgs>({
      query: ({ query, args }) => ({
        url: `/v2/queries/${query}/commands/execute?${encodeArgs(args)}`,
        method: "GET",
      }),
      invalidatesTags: ["Logs"],
    }),
    executeNagiosQuery: builder.mutation<EndpointList, string>({
      query: (id) => ({
        url: `/v2/queries/${id}/commands/execute_nagios`,
        method: "GET",
      }),
      invalidatesTags: ["Logs"],
    }),
    getScripts: builder.query<Script[], void>({
      query: () => ({
        url: "/v2/scripts",
      }),
      providesTags: ["Scripts"],
    }),
    getSettingsStatus: builder.query<SettingsStatus, void>({
      query: () => ({
        url: "/v2/settings/status",
      }),
      providesTags: ["SettingsStatus"],
    }),
    getSettingsDiff: builder.query<SettingsDiff, void>({
      query: () => ({
        url: "/v2/settings/diff",
      }),
      providesTags: ["SettingsStatus", "Settings"],
    }),
    getSettings: builder.query<Settings[], void>({
      query: () => ({
        url: "/v2/settings",
      }),
      providesTags: ["Settings"],
    }),
    getSettingsDescriptions: builder.query<SettingsDescription[], void>({
      query: () => ({
        url: "/v2/settings/descriptions",
      }),
      providesTags: ["SettingsDescriptions"],
    }),
    updateSettings: builder.mutation<string, Settings>({
      query: (settings) => ({
        url: `/v2/settings`,
        method: "PUT",
        body: {
          ...settings,
        },
      }),
      invalidatesTags: () => [
        { type: "Settings" },
        { type: "SettingsStatus" },
        { type: "SettingsDescriptions" },
      ],
    }),
    // DELETE /v2/settings<path>            -> remove the entire path
    // DELETE /v2/settings<path>?key=<name> -> remove a single key
    deleteSettings: builder.mutation<void, { path: string; key?: string }>({
      query: ({ path, key }) => ({
        url: `/v2/settings${path}${key ? `?key=${encodeURIComponent(key)}` : ""}`,
        method: "DELETE",
        // The settings DELETE endpoint may return an empty body or plain text
        // on success. The default responseHandler calls response.json() which
        // throws on an empty body — handle both cases gracefully.
        responseHandler: async (response: Response) => {
          const text = await response.text();
          if (!text) return undefined;
          try {
            return JSON.parse(text);
          } catch {
            return text;
          }
        },
      }),
      invalidatesTags: () => [
        { type: "Settings" },
        { type: "SettingsStatus" },
        { type: "SettingsDescriptions" },
      ],
    }),
    settingsCommand: builder.mutation<string, SettingsCommand>({
      query: (settings) => ({
        url: `/v2/settings/command`,
        method: "POST",
        body: {
          ...settings,
        },
      }),
      invalidatesTags: () => [
        { type: "Settings" },
        { type: "SettingsStatus" },
        { type: "SettingsDescriptions" },
        { type: "Queries" },
        { type: "Modules" },
      ],
    }),
    login: builder.mutation<string, { username: string; password: string }>({
      query: ({ username, password }) => ({
        headers: {
          Authorization: `Basic ${btoa(`${username}:${password}`)}`,
        },
        url: "/v2/login",
        method: "GET",
        responseHandler: async (response: Response) => {
          if (!response.ok) {
            throw new Error("Failed to login");
          }
          const payload = await response.json();
          return payload.key;
        },
      }),
    }),
    getMetrics: builder.query<Metrics, void>({
      query: () => ({
        url: "/v2/metrics",
      }),
      providesTags: ["Metrics"],
    }),
    getEvents: builder.query<EventEntry[], void>({
      query: () => ({
        url: "/v2/events",
      }),
      providesTags: ["Events"],
    }),
    clearEvents: builder.mutation<void, void>({
      query: () => ({
        url: "/v2/events",
        method: "DELETE",
        responseHandler: async (response: Response) => {
          const text = await response.text();
          if (!text) return undefined;
          try {
            return JSON.parse(text);
          } catch {
            return text;
          }
        },
      }),
      invalidatesTags: ["Events"],
    }),
    getCounterMetadata: builder.query<MetadataOption[], void>({
      query: () => ({
        url: "/v1/metadata/counters",
      }),
      transformResponse: (raw: string[]) =>
        Array.isArray(raw) ? raw.map((name) => ({ value: name, label: name })) : [],
    }),
    getChannelMetadata: builder.query<MetadataOption[], void>({
      query: () => ({
        url: "/v2/metadata/channels",
      }),
      transformResponse: (raw: { name: string; plugins?: string[] }[]) => {
        const channels = Array.isArray(raw)
          ? raw.map((c) => ({
              value: c.name,
              label:
                c.plugins && c.plugins.length > 0
                  ? `${c.name} (${c.plugins.join(", ")})`
                  : c.name,
            }))
          : [];
        // Synthetic destinations (not registered as channels) — surface them
        // at the top so users can find them without scrolling.
        return [
          { value: "noop", label: "noop (drop the message)" },
          { value: "event", label: "event (raise an event)" },
          ...channels,
        ];
      },
    }),
  }),
});

export const {
  useGetEndpointsQuery,
  useGetInfoQuery,
  useGetVersionQuery,
  useGetLogsQuery,
  useGetModulesQuery,
  useGetModuleQuery,
  useGetQueriesQuery,
  useGetAliasesQuery,
  useGetQueryQuery,
  useExecuteQueryMutation,
  useExecuteNagiosQueryMutation,
  useGetScriptsQuery,
  useGetSettingsStatusQuery,
  useGetSettingsQuery,
  useGetSettingsDescriptionsQuery,
  useGetSettingsDiffQuery,
  useUpdateSettingsMutation,
  useDeleteSettingsMutation,
  useSettingsCommandMutation,
  useUnloadModuleMutation,
  useLoadModuleMutation,
  useEnableModuleMutation,
  useDisableModuleMutation,
  useGetLogStatusQuery,
  useResetLogStatusMutation,
  useLoginMutation,
  useGetMetricsQuery,
  useGetCounterMetadataQuery,
  useGetChannelMetadataQuery,
  useGetEventsQuery,
  useClearEventsMutation,
} = nsclientApi;
