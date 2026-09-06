#### About `submit_icinga`

`submit_icinga` submits a passive check result to an **Icinga 2** server through
its REST API (`/v1/actions/process-check-result`), over HTTPS.

The usual way to use it is to route results rather than call it by hand: give a
scheduled check `target=icinga`, or add the module's channel to the channels a
check reports on, and each result is submitted as it is produced. A direct call
is mainly useful for verifying that credentials, TLS and object names are right.

##### Matching Icinga's object model

Icinga addresses results by host and service *object*, so a submission only
lands if the objects already exist. `check source` sets the source name recorded
against the result and `check command` the command name it is attributed to.

`ensure objects` makes the module create a missing host or service object before
submitting, using `host template` and `service template` as the templates to
apply. That is convenient for a fleet that registers itself, but it means an
agent can create objects in your monitoring configuration — enable it
deliberately, and give the API user only the permissions it needs.

##### Authentication and TLS

Access uses an Icinga 2 API user: `username` and `password`. TLS is on by
definition (the API is HTTPS-only); `tls version` defaults to 1.3 and `ca`
points at the bundle used to verify the server, defaulting to the system CA
path. Icinga's API certificate is usually issued by the Icinga CA rather than a
public one, so point `ca` at `/var/lib/icinga2/ca/ca.crt` (or wherever your CA
lives) rather than turning verification off.

The API listens on **5665** by default, which is the port to set unless you have
moved it.
