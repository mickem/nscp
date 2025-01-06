import { createApi, fetchBaseQuery, FetchBaseQueryMeta } from "@reduxjs/toolkit/query/react";
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

export const responseHandler = (response: Response): Promise<any> => {
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
  critical: number;
  maximum: number;
  minimum: number;
  unit: string;
  value: number;
  warning: number;
}

export interface QueryExecutionResultLine {
  message: string;
  perf: {
    [key: string]: QueryExecutionResultLinePerf;
  };
}
export interface QueryExecutionResult {
  command: string;
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
  responseHandler: (response: Response): Promise<any> => {
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

const baseQueryWithAuthFail = async (args: any, api: any, extraOptions: any) => {
  const result = await baseQuery(args, api, extraOptions);
  if (result.error && result.error.status === 403) {
    api.dispatch(authSlice.actions.removeToken());
  }
  return result;
};

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
    "Scripts",
    "Settings",
    "SettingsStatus",
    "SettingsDescriptions",
    "LogStatus",
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
      invalidatesTags: (_result, _error, id) => [
        { type: "Module", id },
        { type: "Modules" },
        { type: "Settings" },
        { type: "SettingsDescriptions" },
        { type: "Queries" },
      ],
    }),
    unloadModule: builder.mutation<string, string>({
      query: (id) => ({
        url: `/v2/modules/${id}/commands/unload`,
        method: "GET",
      }),
      invalidatesTags: (_result, _error, id) => [
        { type: "Module", id },
        { type: "Modules" },
        { type: "Settings" },
        { type: "SettingsDescriptions" },
        { type: "Queries" },
      ],
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
      invalidatesTags: (_result, _error, _id) => [
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
      invalidatesTags: (_result, _error, _id) => [
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
        responseHandler: async (response: any) => {
          if (!response.ok) {
            throw new Error("Invalid login");
          }
          const payload = await response.json();
          return payload.key;
        },
      }),
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
  useGetQueryQuery,
  useExecuteQueryMutation,
  useExecuteNagiosQueryMutation,
  useGetScriptsQuery,
  useGetSettingsStatusQuery,
  useGetSettingsQuery,
  useGetSettingsDescriptionsQuery,
  useUpdateSettingsMutation,
  useSettingsCommandMutation,
  useUnloadModuleMutation,
  useLoadModuleMutation,
  useEnableModuleMutation,
  useDisableModuleMutation,
  useGetLogStatusQuery,
  useResetLogStatusMutation,
  useLoginMutation,
} = nsclientApi;
