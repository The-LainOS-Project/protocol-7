/*
 * lainos-init.c – Protocol 7 Session Initializer (v4.4)
 *
 * Unprivileged session bootstrap for greetd/tuigreet.
 * Sets up XDG environment, D-Bus, Wayland, and launches compositor.
 *
 * Hardened: refuses root, TOCTOU-safe directory creation,
 *           compositor fallback chain, Firefox Wayland support.
 *           v4.4: preserves critical inherited env, fixes DBUS addr bug
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>

/* Environment variables to preserve from parent (greetd, ssh, etc.) */
static const char *preserve_env[] = {
    "LANG", "LC_ALL", "LC_CTYPE", "LC_NUMERIC", "LC_TIME", "LC_COLLATE",
    "LC_MONETARY", "LC_MESSAGES", "LC_PAPER", "LC_NAME", "LC_ADDRESS",
    "LC_TELEPHONE", "LC_MEASUREMENT", "LC_IDENTIFICATION",
    "SSH_AGENT_LAUNCHER", "SSH_AUTH_SOCK",
    "GNUPGHOME", "GPG_AGENT_INFO",
    "XKB_DEFAULT_LAYOUT", "XKB_DEFAULT_VARIANT", "XKB_DEFAULT_OPTIONS",
    "P7_COMPOSITOR", "MOZ_ENABLE_WAYLAND",
    NULL
};

static void preserve_critical_env(void) {
    for (int i = 0; preserve_env[i]; i++) {
        const char *val = getenv(preserve_env[i]);
        if (val) {
            /* Use setenv with overwrite=0 to avoid clobbering if already set */
            setenv(preserve_env[i], val, 0);
        }
    }
}

int main(int argc, char **argv) {
    (void)argc;

    if (getuid() == 0 || geteuid() == 0) {
        fprintf(stderr, "[P7] Refusing to run as root\n");
        return 1;
    }

    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    if (!pw) {
        fprintf(stderr, "[P7] getpwuid failed: %m\n");
        return 1;
    }

    /* TOCTOU-safe: openat + mkdirat + fchown */
    int run_fd = open("/run", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (run_fd < 0) {
        fprintf(stderr, "[P7] open /run failed: %m\n");
        return 1;
    }

    char userdir[64];
    snprintf(userdir, sizeof(userdir), "user/%u", uid);

    int user_fd = openat(run_fd, userdir,
                         O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (user_fd < 0 && errno == ENOENT) {
        if (mkdirat(run_fd, userdir, 0700) == -1 && errno != EEXIST) {
            fprintf(stderr, "[P7] mkdirat failed: %m\n");
            close(run_fd);
            return 1;
        }
        user_fd = openat(run_fd, userdir,
                         O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    }
    close(run_fd);

    if (user_fd < 0) {
        fprintf(stderr, "[P7] openat /run/user/%u failed: %m\n", uid);
        return 1;
    }

    if (fchown(user_fd, uid, pw->pw_gid) == -1) {
        fprintf(stderr, "[P7] fchown failed: %m\n");
        close(user_fd);
        return 1;
    }
    close(user_fd);

    char xdg_runtime[128];
    int n = snprintf(xdg_runtime, sizeof(xdg_runtime), "/run/user/%u", uid);
    if (n < 0 || (size_t)n >= sizeof(xdg_runtime)) {
        fprintf(stderr, "[P7] XDG_RUNTIME_DIR path overflow\n");
        return 1;
    }

    /* Compositor selection: env override -> sway -> dwl -> river */
    const char *compositor = getenv("P7_COMPOSITOR");
    if (!compositor || !compositor[0]) {
        if (access("/usr/bin/sway", X_OK) == 0)
            compositor = "sway";
        else if (access("/usr/bin/dwl", X_OK) == 0)
            compositor = "dwl";
        else if (access("/usr/bin/river", X_OK) == 0)
            compositor = "river";
        else {
            fprintf(stderr, "[P7] No compositor found (tried sway, dwl, river)\n");
            return 1;
        }
    }

    /* Build D-Bus address once, correctly */
    char dbus_addr[256];
    n = snprintf(dbus_addr, sizeof(dbus_addr), "unix:path=%s/bus", xdg_runtime);
    if (n < 0 || (size_t)n >= sizeof(dbus_addr)) {
        fprintf(stderr, "[P7] DBUS_SESSION_BUS_ADDRESS path overflow\n");
        return 1;
    }

    /* ─── Environment setup ───
     * Strategy: preserve critical inherited vars, set P7-specific vars,
     *           then overlay with mandatory values.
     */
    preserve_critical_env();

    /* Set/overwrite P7 mandatory environment */
    setenv("PATH", "/usr/local/bin:/usr/bin:/bin", 1);
    setenv("USER", pw->pw_name, 1);
    setenv("LOGNAME", pw->pw_name, 1);
    setenv("HOME", pw->pw_dir, 1);
    setenv("XDG_RUNTIME_DIR", xdg_runtime, 1);
    setenv("XDG_SESSION_TYPE", "wayland", 1);
    setenv("XDG_CURRENT_DESKTOP", compositor, 1);
    setenv("XDG_SESSION_DESKTOP", compositor, 1);
    setenv("WAYLAND_DISPLAY", "wayland-1", 1);
    setenv("DBUS_SESSION_BUS_ADDRESS", dbus_addr, 1);
    setenv("NOTIFY_SOCKET", "/run/systemd/notify", 1);
    setenv("MOZ_ENABLE_WAYLAND", "1", 1);

    /* LIBSEAT_BACKEND is handled by seatd, but keep for compatibility */
    setenv("LIBSEAT_BACKEND", "builtin", 1);

    /* Exec compositor directly — no fork, no zombies */
    execlp(compositor, compositor, (char *)NULL);
    fprintf(stderr, "[P7] exec %s failed: %m\n", compositor);
    return 127;
}