import { Query } from "../services";

export interface QueryState {
  queries?: Query[];
  loading?: boolean;
  error?: String;
  visibilityFilter?: string;
}
  