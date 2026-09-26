/**
 * PythonScript's `fetch_metrics` bridge, end to end.
 *
 * A script returns a dict of readings once per metrics interval. A bare number
 * is a gauge with no description, which is all a script could ever say — so on
 * /api/v2/openmetrics its metrics were names and numbers with nothing telling a
 * scraper what they meant. A value may now be a dict carrying `help`,
 * `unit`, `type` and `labels` alongside the number; this pins both forms, and
 * that the flat JSON view sees the same keys and values either way.
 *
 * A script's metrics arrive under a bundle with no key at all, so the joined
 * name starts with the separator and borrows the `metric_` prefix a name may
 * not begin with an underscore — which is why every family here reads
 * `metric_pyfixture_*`. That predates the metadata work and is unchanged by it.
 */
import * as fs from "fs";
import * as os from "os";
import * as path from "path";

import request from "supertest";

import { NscpInstance, REST_URL, setupQueryNscp, describeIf, hasModule } from "@fixtures/index";

jest.setTimeout(300_000);

const SCRIPT = `
from NSCP import Registry

def my_metrics():
    return {
        # The form that has always worked: a bare number is a gauge, a bare
        # string is a string.
        "pyfixture.plain": 11,
        "pyfixture.status": "ok",
        # The described form.
        "pyfixture.requests": {
            "value": 42,
            "help": "Requests this fixture has handled",
            "type": "counter",
        },
        "pyfixture.size": {
            "value": 4096,
            "help": "Size of the last payload",
            "unit": "bytes",
        },
        # Labels, which is how a script says what a reading is *of*. The key
        # is untouched; only the OpenMetrics endpoint reads them.
        "pyfixture.queue_depth": {
            "value": 5,
            "help": "Messages waiting on the queue",
            "labels": {"queue": "inbound", "region": "eu-west"},
        },
        # A label whose value is not a string, or is empty, is skipped rather
        # than guessed at - an empty label is the same series as no label.
        "pyfixture.partly_labelled": {
            "value": 6,
            "labels": {"good": "yes", "numeric": 8080, "empty": ""},
        },
        # Labels that are not a dict (a list of tuples is the shape people
        # reach for first) are ignored, the metric is still published, and the
        # agent logs why once.
        "pyfixture.bad_labels": {
            "value": 9,
            "labels": [("queue", "inbound")],
        },
        # A type nobody recognises falls back to a gauge rather than vanishing.
        "pyfixture.typo": {"value": 7, "type": "not-a-type"},
        # A dict with no value at all has nothing to publish.
        "pyfixture.novalue": {"help": "no value here"},
    }

def init(plugin_id, plugin_alias, script_alias):
    Registry.get(plugin_id).fetch_metrics(my_metrics)

def shutdown():
    pass
`;

async function getText(key: string, accept: string): Promise<string> {
  const res = await request(REST_URL)
    .get("/api/v2/openmetrics")
    .set("Authorization", `Bearer ${key}`)
    .set("Accept", accept)
    .trustLocalhost(true)
    .buffer(true)
    .parse((r, callback) => {
      let data = "";
      r.on("data", (chunk: Buffer) => (data += chunk.toString()));
      r.on("end", () => callback(null, data));
    })
    .expect(200);
  return res.body as string;
}

async function poll(fetch: () => Promise<string>, until: (v: string) => boolean): Promise<string> {
  const deadline = Date.now() + 60_000;
  let last = "";
  for (;;) {
    last = await fetch();
    if (until(last) || Date.now() >= deadline) return last;
    await new Promise((r) => setTimeout(r, 500));
  }
}

// PythonScript is optional (built where Boost.Python is found; not in the macOS
// package yet), so the suite asks the install rather than assuming.
describeIf(hasModule("PythonScript"))("PythonScript metrics", () => {
  let nscp: NscpInstance;
  let key: string;

  beforeAll(async () => {
    // `${scripts}` defaults to the build's own scripts folder, and that is
    // where PythonScript resolves a relative script name from - so point it at
    // this test's scratch dir rather than dropping a fixture into the build.
    const workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-py-metrics-"));
    const scriptDir = path.join(workDir, "scripts");
    fs.mkdirSync(path.join(scriptDir, "python"), { recursive: true });
    fs.writeFileSync(path.join(scriptDir, "python", "metrics_fixture.py"), SCRIPT);
    nscp = new NscpInstance({ workDir, pathOverrides: { scripts: scriptDir } });

    key = await setupQueryNscp(nscp, "PythonScript", {
      "/modules": { PythonScript: "enabled", WEBServer: "enabled" },
      "/settings/core": { "metrics interval": "1s" },
      "/settings/python/scripts": { metrics_fixture: "metrics_fixture.py" },
    });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("publishes a script's metrics with the metadata the script declared", async () => {
    const text = await poll(
      () => getText(key, "application/openmetrics-text;version=1.0.0"),
      (t) => /^metric_pyfixture_plain /m.test(t),
    );

    // The bare number: a gauge, no help, no unit - exactly as before.
    expect(text).toMatch(/^# TYPE metric_pyfixture_plain gauge$/m);
    expect(text).toMatch(/^metric_pyfixture_plain 11$/m);
    expect(text).not.toMatch(/^# HELP metric_pyfixture_plain/m);

    // The described counter. The family carries the help and the type; the
    // sample carries the `_total` suffix the spec reserves for a counter.
    expect(text).toMatch(/^# HELP metric_pyfixture_requests Requests this fixture has handled$/m);
    expect(text).toMatch(/^# TYPE metric_pyfixture_requests counter$/m);
    expect(text).toMatch(/^metric_pyfixture_requests_total 42$/m);

    // The unit, which also renames the family - OpenMetrics requires a family
    // that declares a unit to end in it.
    expect(text).toMatch(/^# HELP metric_pyfixture_size_bytes Size of the last payload$/m);
    expect(text).toMatch(/^# UNIT metric_pyfixture_size_bytes bytes$/m);
    expect(text).toMatch(/^metric_pyfixture_size_bytes 4096$/m);

    // An unrecognised type is reported as a gauge rather than dropped.
    expect(text).toMatch(/^# TYPE metric_pyfixture_typo gauge$/m);
    expect(text).toMatch(/^metric_pyfixture_typo 7$/m);

    // The labels a script declared reach the sample, in the order it wrote
    // them, and nothing else does.
    expect(text).toMatch(/^metric_pyfixture_queue_depth\{queue="inbound",region="eu-west"\} 5$/m);
    expect(text).toMatch(/^# HELP metric_pyfixture_queue_depth Messages waiting on the queue$/m);
    // A non-string label value and an empty one are both dropped; the usable
    // one still lands.
    expect(text).toMatch(/^metric_pyfixture_partly_labelled\{good="yes"\} 6$/m);
    // Labels that are not a dict at all: the reading still publishes, without
    // them, rather than the whole metric vanishing.
    expect(text).toMatch(/^metric_pyfixture_bad_labels 9$/m);

    // A dict with no `value` has nothing to publish, and must not leave a
    // keyed metric with no sample behind.
    expect(text).not.toMatch(/pyfixture_novalue/);

    // The string folds into the bundle's info family, as any string does. The
    // bundle has no key, so the family falls back to the placeholder name.
    expect(text).toMatch(/^metric_info\{pyfixture_status="ok"\} 1$/m);
  });

  it("leaves the flat JSON view reading exactly what it always did", async () => {
    const res = await request(REST_URL)
      .get("/api/v2/metrics")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
    const metrics = JSON.parse(res.text) as Record<string, unknown>;

    // Same keys, same values, whichever form the script used - the metadata is
    // read by the OpenMetrics endpoint alone. The keyless bundle contributes
    // the leading dot, which is what a script's keys have always looked like
    // here.
    expect(metrics[".pyfixture.plain"]).toBe(11);
    expect(metrics[".pyfixture.requests"]).toBe(42);
    expect(metrics[".pyfixture.size"]).toBe(4096);
    expect(metrics[".pyfixture.status"]).toBe("ok");
    // A labelled metric keeps its key here too: the labels are additive, so a
    // script gains them without its Graphite path or its dashboard moving.
    expect(metrics[".pyfixture.queue_depth"]).toBe(5);
    expect(metrics[".pyfixture.partly_labelled"]).toBe(6);
    expect(metrics[".pyfixture.bad_labels"]).toBe(9);
    expect(metrics[".pyfixture.novalue"]).toBeUndefined();
  });
});
