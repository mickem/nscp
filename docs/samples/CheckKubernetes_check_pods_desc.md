#### About `check_pods`

`check_pods` lists pods - across the cluster or in the given `namespace=`
(repeatable) - and evaluates each one. The list is fetched page by page
(`limit=500`, following `metadata.continue`), so it works on large clusters;
`label-selector=` and `field-selector=` are handed to the API server, so the
filtering happens before the payload is built.

`pod_status` reproduces the STATUS column of `kubectl get pods` (`Running`,
`Completed`, `CrashLoopBackOff`, `ImagePullBackOff`, `OOMKilled`,
`Terminating`, `Init:1/2`, `ExitCode:3`, ...) from the container statuses and
`deletionTimestamp`. Use it rather than `phase` for alerting: a crash-looping
pod has phase `Running`.

Three ways to use it:

* No arguments - every pod that has not finished (`filter=phase !=
  'Succeeded'`). The defaults warn on `Pending` pods and more than five
  restarts, and go critical on `Failed`, `Unknown`, any `*BackOff`,
  `OOMKilled` or a `missing` pod.
* `pod=<name>` or `pod=<namespace>/<name>` (repeatable) - only the named pods
  take part, and a pod the API server does not return is reported with
  `pod_status` `missing`, so it trips the default critical instead of silently
  disappearing from the listing. A bare name is matched in every namespace
  listed and, when missing, reported as `*/<name>` (or under the one
  `namespace=` the check was scoped to).
* A selector - `label-selector=app=web` or `field-selector=spec.nodeName=worker-1`
  for the pods of one application or one node.

`age` takes duration units (`age < 10m`), `created` is a date, and `oom_killed`
is 1 when a container's current or last termination was an out-of-memory kill,
even if the pod has since restarted and shows `Running`. Native sidecars (init
containers with `restartPolicy: Always`) count as running once started and are
included in `containers` / `ready_containers`, as in kubectl; a finished job
pod being cleaned up keeps `Completed` rather than turning into `Terminating`,
while `terminating` still reports the pending deletion.
