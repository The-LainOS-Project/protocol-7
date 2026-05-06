/*
 * lainos-audio-init.c – Protocol 7 Sovereign Audio Initialization
 *
 * Orchestrates PipeWire + WirePlumber startup. PipeWire is daemonized
 * without blocking; WirePlumber starts after socket readiness verification.
 *
 * Hardened: fork/execvp only (no system()), readiness polling,
 *           syslog, clear exit codes, zombie prevention via double-fork.
 *
 * Compile:
 *   gcc -O2 -flto -s -Wall -pie -o /usr/libexec/lainos/lainos-audio-init lainos-audio-init.c
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>

#define LOG_IDENT "lainos-audio-init"
#define PIPEWIRE_SOCKET "pipewire-0"
#define MAX_READY_WAIT_MS 5000
#define READY_POLL_MS 50

static void log_msg(int prio, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsyslog(prio, fmt, ap);
    va_end(ap);

    va_start(ap, fmt);
    fprintf(stderr, "[%s] ", LOG_IDENT);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

/* Double-fork daemon spawn: prevents zombies by ensuring the intermediate
 * parent exits immediately, so init reaps the daemon directly. */
static int spawn_daemon(const char *cmd, char *const argv[]) {
    pid_t pid = fork();
    if (pid == -1) {
        log_msg(LOG_ERR, "fork failed for %s: %s", cmd, strerror(errno));
        return -1;
    }
    if (pid == 0) {
        /* First child */
        pid_t sid = setsid();
        if (sid == -1) {
            _exit(127);
        }

        /* Double-fork to prevent zombie acquisition by session leader */
        pid_t pid2 = fork();
        if (pid2 == -1) {
            _exit(127);
        }
        if (pid2 > 0) {
            /* First child exits immediately; second child is reaped by init */
            _exit(0);
        }

        /* Second child (actual daemon) */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_WRONLY);
        open("/dev/null", O_WRONLY);

        signal(SIGCHLD, SIG_IGN);

        execvp(cmd, argv);
        _exit(127);
    }

    /* Parent: reap first child immediately */
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        log_msg(LOG_ERR, "waitpid failed for intermediate child: %s", strerror(errno));
        return -1;
    }
    return 0;
}

/* Poll for PipeWire socket readiness instead of blind sleep */
static int wait_for_pipewire(const char *runtime_dir, int max_ms) {
    char socket_path[128];
    snprintf(socket_path, sizeof(socket_path), "%s/%s", runtime_dir, PIPEWIRE_SOCKET);

    struct stat st;
    int waited = 0;
    while (waited < max_ms) {
        if (stat(socket_path, &st) == 0 && S_ISSOCK(st.st_mode)) {
            log_msg(LOG_INFO, "PipeWire socket ready after %d ms", waited);
            return 0;
        }
        usleep(READY_POLL_MS * 1000);
        waited += READY_POLL_MS;
    }

    log_msg(LOG_WARNING, "PipeWire socket not ready after %d ms, starting WirePlumber anyway", max_ms);
    return -1;
}

int main(void) {
    openlog(LOG_IDENT, LOG_PID | LOG_CONS, LOG_DAEMON);

    uid_t uid = getuid();
    if (uid == 0) {
        log_msg(LOG_CRIT, "Refusing to run as root");
        closelog();
        return 2;
    }

    char runtime[64];
    snprintf(runtime, sizeof(runtime), "/run/user/%u", uid);

    struct stat st;
    if (stat(runtime, &st) == -1) {
        log_msg(LOG_ERR, "XDG_RUNTIME_DIR %s does not exist", runtime);
        closelog();
        return 1;
    }

    log_msg(LOG_INFO, "Starting sovereign audio stack (PipeWire -> WirePlumber)");

    char *pipewire_argv[] = {"pipewire", NULL};
    if (spawn_daemon("pipewire", pipewire_argv) != 0) {
        log_msg(LOG_ERR, "PipeWire failed to start");
        closelog();
        return 1;
    }

    log_msg(LOG_INFO, "PipeWire spawned, waiting for socket readiness...");
    wait_for_pipewire(runtime, MAX_READY_WAIT_MS);

    char *wireplumber_argv[] = {"wireplumber", NULL};
    if (spawn_daemon("wireplumber", wireplumber_argv) != 0) {
        log_msg(LOG_ERR, "WirePlumber failed to start");
        closelog();
        return 1;
    }

    log_msg(LOG_INFO, "Sovereign audio stack initialized");
    closelog();
    return 0;
}
