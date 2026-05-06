/*
 * lainos-init.c – Protocol 7 Sovereign Session Initializer
 *
 * Runs as unprivileged user (from greetd/tuigreet).
 * Clears inherited environment, sets minimal deterministic variables,
 * ensures XDG_RUNTIME_DIR exists and is correctly owned, then execs Sway.
 *
 * Hardened: clearenv isolation, TOCTOU-safe dir creation via openat+mkdirat,
 *           no root ops, syslog audit, direct execvp handoff (no fork/zombies).
 *
 * Compile:
 *   gcc -O2 -flto -s -Wall -pie -o /usr/libexec/lainos/lainos-init lainos-init.c
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <pwd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>

/* TOCTOU-safe directory creation:
 * 1. Open parent directory with O_RDONLY | O_DIRECTORY | O_CLOEXEC
 * 2. Use mkdirat() relative to parent fd
 * 3. Use fchownat() / fchmodat() with AT_SYMLINK_NOFOLLOW
 * This avoids races where an attacker swaps a symlink between check and use.
 */
static int safe_mkdir_p(const char *path, uid_t uid, mode_t mode) {
    int parent_fd = open("/run/user", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (parent_fd == -1) {
        syslog(LOG_ERR, "Cannot open /run/user: %s", strerror(errno));
        return -1;
    }

    char uid_str[32];
    snprintf(uid_str, sizeof(uid_str), "%u", uid);

    int r = mkdirat(parent_fd, uid_str, mode);
    if (r == -1 && errno != EEXIST) {
        syslog(LOG_ERR, "mkdirat /run/user/%s failed: %s", uid_str, strerror(errno));
        close(parent_fd);
        return -1;
    }

    int rdir_fd = openat(parent_fd, uid_str, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    close(parent_fd);

    if (rdir_fd == -1) {
        syslog(LOG_ERR, "openat /run/user/%s failed: %s", uid_str, strerror(errno));
        return -1;
    }

    struct stat st;
    if (fstat(rdir_fd, &st) == -1) {
        syslog(LOG_ERR, "fstat /run/user/%s failed: %s", uid_str, strerror(errno));
        close(rdir_fd);
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        syslog(LOG_ERR, "/run/user/%s is not a directory", uid_str);
        close(rdir_fd);
        return -1;
    }

    if (fchown(rdir_fd, uid, uid) == -1) {
        syslog(LOG_WARNING, "fchown /run/user/%s failed: %s", uid_str, strerror(errno));
    }
    if (fchmod(rdir_fd, mode) == -1) {
        syslog(LOG_WARNING, "fchmod /run/user/%s failed: %s", uid_str, strerror(errno));
    }

    close(rdir_fd);
    syslog(LOG_INFO, "XDG_RUNTIME_DIR /run/user/%s ready", uid_str);
    return 0;
}

int main(int argc, char *argv[]) {
    openlog("lainos-init", LOG_PID | LOG_CONS, LOG_USER);

    uid_t uid = getuid();
    if (uid == 0) {
        syslog(LOG_CRIT, "Refusing to run as root");
        fprintf(stderr, "[Protocol 7] Refusing to run as root\n");
        closelog();
        return 1;
    }

    struct passwd *pw = getpwuid(uid);
    if (!pw) {
        syslog(LOG_CRIT, "Cannot resolve user for UID %u", uid);
        fprintf(stderr, "[Protocol 7] Cannot resolve user (UID %u)\n", uid);
        closelog();
        return 1;
    }

    if (safe_mkdir_p("/run/user", uid, 0700) != 0) {
        syslog(LOG_ERR, "Failed to create XDG_RUNTIME_DIR, attempting fallback");
        /* Fallback: try direct mkdir (less safe but may work on systems
         * where /run/user doesn't exist yet) */
        char runtime[64];
        snprintf(runtime, sizeof(runtime), "/run/user/%u", uid);
        if (mkdir(runtime, 0700) == -1 && errno != EEXIST) {
            syslog(LOG_CRIT, "Fallback mkdir %s failed: %s", runtime, strerror(errno));
            fprintf(stderr, "[Protocol 7] Cannot create runtime dir %s\n", runtime);
            closelog();
            return 1;
        }
        if (chown(runtime, uid, uid) == -1 || chmod(runtime, 0700) == -1) {
            syslog(LOG_WARNING, "Fallback chown/chmod %s failed: %s", runtime, strerror(errno));
        }
    }

    if (clearenv() != 0) {
        syslog(LOG_WARNING, "clearenv failed: %s (continuing)", strerror(errno));
    }

    setenv("PATH", "/usr/local/bin:/usr/bin:/bin", 1);
    setenv("USER", pw->pw_name, 1);
    setenv("HOME", pw->pw_dir, 1);

    char runtime[64];
    snprintf(runtime, sizeof(runtime), "/run/user/%u", uid);
    setenv("XDG_RUNTIME_DIR", runtime, 1);
    setenv("XDG_SESSION_TYPE", "wayland", 1);
    setenv("XDG_CURRENT_DESKTOP", "sway", 1);

    char dbus_addr[128];
    snprintf(dbus_addr, sizeof(dbus_addr), "unix:path=%s/bus", runtime);
    setenv("DBUS_SESSION_BUS_ADDRESS", dbus_addr, 1);

    setenv("NOTIFY_SOCKET", "/run/systemd/notify", 1);
    setenv("LIBSEAT_BACKEND", "builtin", 1);

    printf("[Protocol 7] Environment sanitized for user: %s (UID %u)\n", pw->pw_name, uid);
    printf("[Protocol 7] Launching Sway (sovereign standalone mode)...\n");

    char *sway_argv[] = {"sway", NULL};
    if (execvp("sway", sway_argv) == -1) {
        syslog(LOG_CRIT, "execvp sway failed: %s", strerror(errno));
        fprintf(stderr, "[Protocol 7] Failed to launch Sway: %s\n", strerror(errno));
        closelog();
        return 127;
    }

    return 0;
}
