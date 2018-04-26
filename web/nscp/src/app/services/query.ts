export interface Query {
  title: string;
  name: string;
  description: string;
  metadata: Map<string,string>;
  plugin: string;
}

export interface FullQuery  extends Query {
}

export interface ResponsePerf {
  value: number;
  unit: string;
  minimum: number;
  maximum: number;
  warning: number;
  critical: number;
}
export interface ResponseLine {
  message: string;
  perf: Map<string,ResponsePerf>;
  result: number;
}
export interface QueryResponse {
  command: string;
  lines: ResponseLine[];
  result: number;
}