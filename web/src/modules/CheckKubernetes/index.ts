import type { ModulePage } from "../types.ts";
import { tableKeys } from "../kit/keys.ts";
import KubernetesPage from "./KubernetesPage.tsx";
import { KUBE_TABLE } from "./kubernetes.ts";

const CheckKubernetes: ModulePage = {
  id: "CheckKubernetes",
  Panel: KubernetesPage,
  handledKeys: tableKeys(KUBE_TABLE),
};

export default CheckKubernetes;
