/*
 * lainos-ghost-units.c – Protocol 7 Ghost Directory Creator
 *
 * Creates volatile /run/systemd/* paths that AUR packages and desktop
 * applications expect to exist. Run at boot or session start.
 *
 * Hardened: mkdir only, no shell, minimal privileges, idempotent.
 *
 * Compile:
 *   gcc -O2 -flto -s -Wall -pie -o /usr/libexec/lainos/lainos-ghost-units lainos-ghost-units.c
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <syslog.h>
#include <errno.h>

#define LOG_IDENT "lainos-ghost-units"

static const char *ghost_dirs[] = {
    "/run/systemd/system",
    "/run/systemd/notify",
    "/run/systemd/private",
    "/run/systemd/users",
    "/run/systemd/sessions",
    "/run/systemd/machines",
    "/run/systemd/shutdown",
    "/run/systemd/inhibit",
    NULL
};

int main(void) {
    openlog(LOG_IDENT, LOG_PID | LOG_CONS, LOG_DAEMON);

    if (getuid() != 0) {
        syslog(LOG_CRIT, "Must run as root");
        closelog();
        return 2;
    }

    int failed = 0;

    for (int i = 0; ghost_dirs[i]; i++) {
        if (mkdir(ghost_dirs[i], 0755) == -1 && errno != EEXIST) {
            syslog(LOG_ERR, "Failed to create %s: %s", ghost_dirs[i], strerror(errno));
            failed++;
        } else {
            syslog(LOG_INFO, "Ghost dir ready: %s", ghost_dirs[i]);
        }
    }

    syslog(LOG_NOTICE, "Ghost units initialized (%d/%d created)",
           (int)(sizeof(ghost_dirs)/sizeof(ghost_dirs[0]) - 1) - failed,
           (int)(sizeof(ghost_dirs)/sizeof(ghost_dirs[0]) - 1));

    closelog();
    return failed ? 1 : 0;
}
