#!/bin/sh
# Run NSClient++ from the build tree mounted at $NSCP_BUILD_DIR, against the master.
# `nscp test` keeps running while its stdin stays open.
tail -f /dev/null | "$NSCP_BUILD_DIR/nscp" test --settings ini:///etc/nscp/nsclient.ini \
  --define "/settings/Icinga/agent:parent host=${ICINGA_PARENT_HOST:-icinga-master}" \
  --define "/settings/Icinga/agent:ticket=${ICINGA_TICKET}"
