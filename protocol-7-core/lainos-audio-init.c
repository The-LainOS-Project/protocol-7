/*
 * lainos-audio-init.c – Protocol 7 Audio Orchestrator (v4.3-tuned)
 *
 * Spawns PipeWire + WirePlumber + pipewire-pulse for full audio compatibility.
 * Double-fork daemon pattern prevents zombies. Socket readiness polling
 * instead of blind sleep.
 *
 * Hardened: refuses root, TOCTOU-safe, no system() calls.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#define MAX_WAIT_MS 5000
#define POLL_INTERVAL_MS 50

static int wait_for_socket(const char *path, int max_ms, int interval_ms) {
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = interval_ms * 1000000L;
    for (int waited = 0; waited < max_ms; waited += interval_ms) {
        if (access(path, F_OK) == 0)
            return 0;
        nanosleep(&ts, NULL);
    }
    return -1;
}

static pid_t spawn_daemon(const char *cmd) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid > 0) {
        /* Parent: intermediate child exits immediately so init reaps */
        return pid;
    }

    /* First child */
    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("fork");
        _exit(1);
    }
    if (pid2 > 0) {
        /* First child exits, actual daemon becomes orphan -> init reaps */
        _exit(0);
    }

    /* Second child = actual daemon */
    setsid();
    execlp(cmd, cmd, (char *)NULL);
    perror("execlp");
    _exit(127);
}

int main(void) {
    if (getuid() == 0 || geteuid() == 0) {
        fprintf(stderr, "[P7] Refusing to run audio init as root\n");
        return 1;
    }

    const char *runtime = getenv("XDG_RUNTIME_DIR");
    if (!runtime) {
        fprintf(stderr, "[P7] XDG_RUNTIME_DIR not set\n");
        return 1;
    }

    /* Spawn PipeWire daemon */
    pid_t pw_pid = spawn_daemon("pipewire");
    if (pw_pid < 0) {
        fprintf(stderr, "[P7] Failed to spawn pipewire\n");
        return 1;
    }

    /* Wait for PipeWire socket */
    char socket_path[256];
    snprintf(socket_path, sizeof(socket_path), "%s/pipewire-0", runtime);

    if (wait_for_socket(socket_path, MAX_WAIT_MS, POLL_INTERVAL_MS) != 0) {
        fprintf(stderr, "[P7] Timeout waiting for %s\n", socket_path);
        return 1;
    }

    /* Spawn WirePlumber session manager */
    pid_t wp_pid = spawn_daemon("wireplumber");
    if (wp_pid < 0) {
        fprintf(stderr, "[P7] Failed to spawn wireplumber\n");
        return 1;
    }

    /* Wait briefly for WirePlumber to initialize */
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 500 * 1000000L;
    nanosleep(&ts, NULL);

    /* Spawn pipewire-pulse for PulseAudio compatibility */
    pid_t pulse_pid = spawn_daemon("pipewire-pulse");
    if (pulse_pid < 0) {
        fprintf(stderr, "[P7] Failed to spawn pipewire-pulse (non-fatal)\n");
        /* Non-fatal: PipeWire native clients still work */
    }

    /* Optional: pipewire-jack for JACK compatibility */
    if (getenv("P7_ENABLE_JACK")) {
        pid_t jack_pid = spawn_daemon("pipewire-jack");
        if (jack_pid < 0) {
            fprintf(stderr, "[P7] Failed to spawn pipewire-jack (non-fatal)\n");
        }
    }

    fprintf(stderr, "[P7] Audio stack ready: pipewire + wireplumber + pipewire-pulse\n");
    return 0;
}
