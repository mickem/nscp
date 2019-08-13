import { LogEntry } from "../services";

export interface LogState {
  log?: LogEntry[];
  loading?: boolean;
  error?: String;
  visibilityFilter?: string;
}
  