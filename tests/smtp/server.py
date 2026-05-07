"""
Tiny SMTP server used by tests/smtp/run-test.bat to verify the SMTPClient
module against a real SMTP submission flow (AUTH + STARTTLS + DATA).

The server:
  * listens on 1025 (plain, STARTTLS-capable) and 1465 (implicit TLS)
  * advertises AUTH PLAIN / AUTH LOGIN
  * accepts a single hardcoded credential pair from $SMTP_USERNAME / $SMTP_PASSWORD
  * writes every accepted DATA payload to /inbox/messages.txt with a clear
    delimiter so the host-side test can findstr through it

This is intentionally tiny; aiosmtpd carries the heavy lifting. We avoid
any production hardening because the goal is integration testing, not a
real relay.
"""

import asyncio
import os
import ssl
import sys
from aiosmtpd.controller import Controller
from aiosmtpd.handlers import Message
from aiosmtpd.smtp import SMTP, AuthResult, LoginPassword


CERT = "/tmp/smtp-cert/server.crt"
KEY = "/tmp/smtp-cert/server.key"
INBOX = "/inbox/messages.txt"

USERNAME = os.environ.get("SMTP_USERNAME", "alerts@example.com")
PASSWORD = os.environ.get("SMTP_PASSWORD", "change_me")


def auth_callback(server, session, envelope, mechanism, auth_data):
    if isinstance(auth_data, LoginPassword):
        username = auth_data.login.decode("utf-8", "replace")
        password = auth_data.password.decode("utf-8", "replace")
    else:
        return AuthResult(success=False, handled=True)
    if username == USERNAME and password == PASSWORD:
        return AuthResult(success=True)
    return AuthResult(success=False, handled=True)


class CapturingHandler:
    """Append every accepted message to /inbox/messages.txt with a delimiter
    that the test script greps for. Header preservation matters: the test
    asserts on Subject and a CRLF-injection negative case."""

    async def handle_DATA(self, server, session, envelope):
        body = envelope.content.decode("utf-8", "replace")
        with open(INBOX, "a", encoding="utf-8") as fh:
            fh.write("===== BEGIN MESSAGE =====\n")
            fh.write(f"AUTH={session.authenticated}\n")
            fh.write(f"PEER={session.peer}\n")
            fh.write(f"MAIL_FROM={envelope.mail_from}\n")
            for r in envelope.rcpt_tos:
                fh.write(f"RCPT_TO={r}\n")
            fh.write("--- DATA ---\n")
            fh.write(body)
            if not body.endswith("\n"):
                fh.write("\n")
            fh.write("===== END MESSAGE =====\n")
            fh.flush()
        return "250 OK"


def build_tls_context():
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(CERT, KEY)
    return ctx


# Subclass Controller to inject auth + STARTTLS into the SMTP factory. The
# default Controller.factory() just builds SMTP(self.handler), so overriding
# the method is the documented way to pass the extra knobs (tls_context
# for STARTTLS, authenticator for AUTH).
class StarttlsController(Controller):
    def factory(self):
        return SMTP(
            self.handler,
            require_starttls=False,
            tls_context=build_tls_context(),
            authenticator=auth_callback,
            auth_required=False,
            # Test 4 sends auth in clear so we can prove the *client* refuses;
            # don't make the server reject it pre-emptively.
            auth_require_tls=False,
        )


class ImplicitTlsController(Controller):
    """Implicit TLS port. Controller's `ssl_context` ctor arg wraps the
    listening socket with TLS; the SMTP() factory inside still has to
    register the auth callback."""
    def factory(self):
        return SMTP(
            self.handler,
            authenticator=auth_callback,
            auth_required=False,
            auth_require_tls=False,
        )


def main():
    plain_ctrl = StarttlsController(
        CapturingHandler(),
        hostname="0.0.0.0",
        port=1025,
        server_hostname="smtp-test",
    )
    tls_ctrl = ImplicitTlsController(
        CapturingHandler(),
        hostname="0.0.0.0",
        port=1465,
        server_hostname="smtp-test",
        ssl_context=build_tls_context(),
    )

    plain_ctrl.start()
    tls_ctrl.start()
    print("smtp-test ready", flush=True)
    try:
        # Block forever.
        asyncio.get_event_loop().run_forever()
    except KeyboardInterrupt:
        pass
    finally:
        plain_ctrl.stop()
        tls_ctrl.stop()


if __name__ == "__main__":
    main()
