#### About `check_ok`

`check_ok` is a constant: it always returns **OK**, without running
anything. It exists so that a monitoring configuration can be exercised
end-to-end — that the transport works, that the command is allowed, that the
server renders the state — without depending on the health of the host.

The only option is `message=`, which sets the text returned alongside the
status. It defaults to `No message`.

The legacy alias `CheckOK` is accepted for backwards compatibility.
