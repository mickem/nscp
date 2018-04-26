export interface Module {
    id: string;
    title: string;
    name: string;
    description: string;
    loaded: boolean;
    enabled: boolean;
    metadata: Map<string,string>;
  }