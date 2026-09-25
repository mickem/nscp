/**
 * CheckKubernetes against a real cluster (kind, k3d, minikube, or anything a
 * kubeconfig reaches). Skipped unless NSCP_KUBECONFIG names a kubeconfig in
 * JSON form - the only form the module reads:
 *
 *   kubectl config view --raw --minify -o json > /tmp/kubeconfig.json
 *   NSCP_KUBECONFIG=/tmp/kubeconfig.json NSCP_SKIP_DOCKER=1 NSCP_BIN=... npx jest --runInBand checkkubernetes-live
 *
 * The assertions are "shape + healthy", not forced WARNING/CRITICAL: the
 * cluster is whatever it is. Threshold behaviour, pagination and the error
 * contract are covered against the fake API server in
 * checkkubernetes-commands.test.ts.
 */
import * as fs from "fs";

import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

const KUBECONFIG = process.env.NSCP_KUBECONFIG ?? "";
const canRun = KUBECONFIG !== "" && fs.existsSync(KUBECONFIG);
const maybeDescribe = canRun ? describe : describe.skip;

maybeDescribe("CheckKubernetes against a live cluster", () => {
  let nscp: NscpInstance;

  async function query(
    command: string,
    args: string[] = [],
  ): Promise<{ out: string; exitCode: number }> {
    const r = await nscp.run(
      ["client", "--module", "CheckKubernetes", "--boot", "--query", command, ...args],
      {
        allowFailure: true,
        timeout: 60_000,
      },
    );
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, exitCode: r.exitCode ?? -1 };
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": { CheckKubernetes: "enabled" },
      "/settings/kubernetes": { kubeconfig: KUBECONFIG },
    });
  });

  it("check_kubernetes reaches the API server and counts the nodes", async () => {
    const { out, exitCode } = await query("check_kubernetes");
    expect(out).toMatch(
      /Kubernetes v\d+\.\d+\.\S+ at https?:\/\/\S+: API (ready|not ready), \d+\/\d+ nodes ready/,
    );
    expect([0, 2]).toContain(exitCode);
  });

  it("check_nodes reports every node with a real status word", async () => {
    const { out, exitCode } = await query("check_nodes", [
      "detail-syntax=%(name)=%(node_status)/%(kubelet_version)",
      "top-syntax=${list}",
      "ok-syntax=",
    ]);
    expect(out).toMatch(/\S+=(Ready|NotReady|Unknown)(,SchedulingDisabled)?\/v\d+\.\d+/);
    expect([0, 1, 2]).toContain(exitCode);
  });

  it("check_pods lists the kube-system pods with the kubectl STATUS column", async () => {
    const { out, exitCode } = await query("check_pods", [
      "namespace=kube-system",
      "detail-syntax=%(namespace)/%(name)=%(pod_status) %(ready_containers)/%(containers)",
      "top-syntax=${list}",
      "ok-syntax=",
      "warning=none",
      "critical=none",
    ]);
    expect(out).toMatch(/kube-system\/\S+=\S+ \d+\/\d+/);
    expect(exitCode).toBe(0);
  });

  it("check_workloads sees the cluster's own daemonsets and deployments", async () => {
    const { out, exitCode } = await query("check_workloads", [
      "namespace=kube-system",
      "detail-syntax=%(kind) %(name)=%(available)/%(desired)",
      "top-syntax=${list}",
      "ok-syntax=",
    ]);
    expect(out).toMatch(/(Deployment|DaemonSet|StatefulSet) \S+=\d+\/\d+/);
    expect([0, 1, 2]).toContain(exitCode);
  });

  it("check_pods pod= reports a pod that does not exist as missing", async () => {
    const { out, exitCode } = await query("check_pods", [
      "namespace=kube-system",
      "pod=nscp-no-such-pod",
    ]);
    expect(out).toMatch(/kube-system\/nscp-no-such-pod=missing/);
    expect(exitCode).toBe(2);
  });
});
