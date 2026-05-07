/*
 * lainos-net-init.c – Protocol 7 Network Hardening + iwd Prep (v4.3-tuned)
 *
 * Applies kernel sysctl hardening for anti-fingerprinting and DoS resistance.
 * Prepares iwd wireless if wlan0 exists. Updates resolvconf.
 *
 * Hardened: runs as root, returns 0 (all success), 1 (partial failure), 2 (fatal).
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

static int write_sysctl(const char *path, const char *value) {
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "[P7] sysctl %s: %m\n", path);
        return -1;
    }
    int r = fputs(value, f);
    fclose(f);
    return (r < 0) ? -1 : 0;
}

static int run_cmd(const char *cmd) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        execlp("sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int main(void) {
    if (getuid() != 0) {
        fprintf(stderr, "[P7] Network init must run as root\n");
        return 2;
    }

    int failed = 0;

    /* ─── IPv4 hardening ─── */
    if (write_sysctl("/proc/sys/net/ipv4/conf/all/rp_filter", "1\n") < 0) failed++;
    if (write_sysctl("/proc/sys/net/ipv4/conf/default/rp_filter", "1\n") < 0) failed++;
    if (write_sysctl("/proc/sys/net/ipv4/conf/all/accept_redirects", "0\n") < 0) failed++;
    if (write_sysctl("/proc/sys/net/ipv4/conf/default/accept_redirects", "0\n") < 0) failed++;
    if (write_sysctl("/proc/sys/net/ipv4/conf/all/secure_redirects", "0\n") < 0) failed++;
    if (write_sysctl("/proc/sys/net/ipv4/conf/all/send_redirects", "0\n") < 0) failed++;
    if (write_sysctl("/proc/sys/net/ipv4/conf/all/accept_source_route", "0\n") < 0) failed++;
    if (write_sysctl("/proc/sys/net/ipv4/tcp_syncookies", "1\n") < 0) failed++;
    if (write_sysctl("/proc/sys/net/ipv4/icmp_echo_ignore_broadcasts", "1\n") < 0) failed++;
    if (write_sysctl("/proc/sys/net/ipv4/conf/all/log_martians", "1\n") < 0) failed++;
    if (write_sysctl("/proc/sys/net/ipv4/icmp_ignore_bogus_error_responses", "1\n") < 0) failed++;
    /* NEW: Disable TCP timestamps to reduce fingerprinting */
    if (write_sysctl("/proc/sys/net/ipv4/tcp_timestamps", "0\n") < 0) {
        fprintf(stderr, "[P7] tcp_timestamps hardening failed (non-fatal)\n");
        /* Non-fatal: some networks require timestamps for TCP performance */
    }

    /* ─── IPv6 hardening ─── */
    if (write_sysctl("/proc/sys/net/ipv6/conf/all/accept_redirects", "0\n") < 0) failed++;
    if (write_sysctl("/proc/sys/net/ipv6/conf/default/accept_redirects", "0\n") < 0) failed++;
    if (write_sysctl("/proc/sys/net/ipv6/conf/all/accept_source_route", "0\n") < 0) failed++;

    /* ─── Wireless prep ─── */
    struct stat st;
    if (stat("/sys/class/net/wlan0", &st) == 0) {
        if (run_cmd("iwctl station wlan0 scan") != 0) {
            fprintf(stderr, "[P7] iwctl scan failed (non-fatal)\n");
        }
    }

    /* ─── DNS resolver update ─── */
    if (run_cmd("resolvconf -u") != 0) {
        fprintf(stderr, "[P7] resolvconf -u failed (non-fatal)\n");
    }

    if (failed > 0) {
        fprintf(stderr, "[P7] Network hardening: %d sysctl(s) failed\n", failed);
        return 1;
    }

    fprintf(stderr, "[P7] Network hardening applied successfully\n");
    return 0;
}
