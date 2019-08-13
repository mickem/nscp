import { Module } from "../services";

export interface ModuleState {
  modules?: Module[];
  loading?: boolean;
  error?: String;
  visibilityFilter?: string;
}
  