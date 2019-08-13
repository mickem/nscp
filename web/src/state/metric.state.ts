import { MetricsDictionary } from "../services";

export interface MetricState {
  metrics?: MetricsDictionary;
  loading?: boolean;
  error?: String;
  visibilityFilter?: string;
}
  