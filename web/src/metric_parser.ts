import { Metrics } from "./api/api.ts";

export interface Metric {
  module: string;
  type?: string;
  key: string;
  metric: string;
  instance?: string;
  value: string | number;
}

export interface Result {
  metrics: Metric[];
  modules: string[];
}

export const parseMetrics = (metrics?: Metrics): Result => {
  const metricsValue = metrics || {};
  const modules: string[] = [];
  const parsedMetrics = Object.keys(metricsValue).map((k) => {
    const parsedKey = k.split(".");
    const module = parsedKey[0];
    const metric = parsedKey[parsedKey.length - 1];
    if (!modules.includes(module)) {
      modules.push(module);
    }

    if (parsedKey.length == 2) {
      return {
        module,
        key: k,
        metric,
        value: metricsValue[k],
      } as Metric;
    }
    const type = parsedKey[1];
    if (parsedKey.length == 3) {
      return {
        module,
        type,
        key: k,
        metric,
        value: metricsValue[k],
      } as Metric;
    }

    const instance = parsedKey.slice(2, parsedKey.length - 1).join(".");
    return {
      module,
      type,
      key: k,
      metric,
      instance,
      value: metricsValue[k],
    } as Metric;
  });
  return {
    metrics: parsedMetrics,
    modules,
  };
};
