# ---------------------------------------------------------------------------
# A Mod-Gearman playground: Naemon + the Mod-Gearman NEB module + gearmand +
# the Thruk web UI, in one container.
#
# This is a toy. It keeps no state, notifies nobody, runs three processes in
# one container and lets anyone who opens the web UI in as an administrator.
# It exists so that an NSClient++ with the GearmanClient module enabled has a
# real monitoring core to answer to, without anyone having to install one. Do
# not put it on a network you do not own.
#
# Everything the core schedules for the hostgroup named in conf/module.conf
# is put on gearmand instead of being run here, and whoever registers on that
# queue answers it - which is the whole point: the agent connects out to
# 4730 and picks the checks up.
# ---------------------------------------------------------------------------
FROM debian:bookworm-slim

# ConSol Labs publishes the Naemon and Mod-Gearman packages. The repository
# signing key is fetched from the same place the repository's own install
# instructions hand it out, and scoped to this list file with signed-by
# rather than trusted for every repository in the image.
ARG CONSOL_KEY=monitoring-repo-consol-de-gpg-2026.asc

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl \
    && curl -fsSL "https://labs.consol.de/repo/stable/${CONSOL_KEY}" \
        -o "/etc/apt/trusted.gpg.d/${CONSOL_KEY}" \
    && echo "deb [signed-by=/etc/apt/trusted.gpg.d/${CONSOL_KEY}]" \
        "https://labs.consol.de/repo/stable/debian bookworm main" \
        > /etc/apt/sources.list.d/labs-consol-stable.list \
    && apt-get update && apt-get install -y --no-install-recommends \
        naemon-core \
        naemon-livestatus \
        mod-gearman-module \
        mod-gearman-tools \
        gearman-job-server \
        gearman-tools \
        thruk-base \
        starman \
        procps \
    && rm -rf /var/lib/apt/lists/*

# The NEB module has moved between /usr/lib/mod_gearman and
# /usr/lib/<triplet>/mod_gearman across releases, and the triplet differs
# between an amd64 and an arm64 build of this image. Link it to a fixed path
# so the entrypoint can name one; dpkg -L is the package's own answer to
# "where did it put it".
RUN mkdir -p /opt/mod_gearman \
    && ln -s "$(dpkg -L mod-gearman-module | grep -m1 'mod_gearman_naemon\.o$')" \
        /opt/mod_gearman/mod_gearman_naemon.o

# The stock sample objects monitor a localhost, a printer and a switch with
# plugins this image does not install; they would sit CRITICAL next to the
# lab's own hosts and distract from them. The templates, timeperiods,
# contacts and command definitions stay - conf/objects.cfg builds on them.
RUN rm -f /etc/naemon/conf.d/localhost.cfg \
          /etc/naemon/conf.d/printer.cfg \
          /etc/naemon/conf.d/switch.cfg \
          /etc/naemon/conf.d/windows.cfg

# Lab tuning of the stock naemon.cfg: the status the web UI reads is
# refreshed every other second rather than every ten, every restart starts
# from nothing, and the first state of every check is logged so
# `docker compose logs` shows the checks arriving. None of this is about
# Mod-Gearman.
RUN printf '%s\n' \
    '' \
    '# --- Added by the NSClient++ Mod-Gearman lab ---' \
    'status_update_interval=2' \
    'log_initial_states=1' \
    'retain_state_information=0' \
    >> /etc/naemon/naemon.cfg

COPY thruk_local.conf /etc/thruk/thruk_local.d/50-gearman-lab.conf
COPY thruk.psgi /opt/thruk.psgi
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# 4730 gearmand (the agent connects here), 8080 Thruk.
EXPOSE 4730 8080

ENTRYPOINT ["/entrypoint.sh"]
