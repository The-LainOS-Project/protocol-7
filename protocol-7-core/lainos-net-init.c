/*
 * lainos-net-init.c – Protocol 7 Sovereign Network Initialization
 *
 * Applies kernel networking hardening (anti-fingerprint, leak prevention) and
 * triggers iwd station scan if wlan0 exists.
 * Runs early in boot/session (OpenRC or lainos-init).
 *
 * Hardened: sysctlbyname (no shell), interface check, dual logging,
 *           clear exit codes (0=success, 1=partial, 2=fatal).
 *           No system() calls — fork/execvp only.
 *
 * Compile:
 *   gcc -O2 -flto -s -Wall -pie -o /usr/libexec/lainos/lainos-net-init lainos-net-init.c
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

#define LOG_IDENT           "lainos-net-init"

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

static int set_sysctl(const char *key, int val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", val);

    if (sysctlbyname(key, NULL, NULL, buf, strlen(buf)) == -1) {
        log_msg(LOG_ERR, "sysctl %s = %d failed: %s", key, val, strerror(errno));
        return -1;
    }
    log_msg(LOG_INFO, "sysctl %s = %d applied", key, val);
    return 0;
}

static int run_command(const char *cmd, char *const argv[]) {
    pid_t pid = fork();
    if (pid == -1) {
        log_msg(LOG_ERR, "fork failed for %s: %s", cmd, strerror(errno));
        return -1;
    }
    if (pid == 0) {
        execvp(cmd, argv);
        _exit(127);
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) {
        log_msg(LOG_ERR, "waitpid failed for %s: %s", cmd, strerror(errno));
        return -1;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return 0;
    }
    return -1;
}

int main(void) {
    openlog(LOG_IDENT, LOG_PID | LOG_CONS, LOG_DAEMON);

    if (getuid() != 0) {
        log_msg(LOG_CRIT, "Must run as root");
        closelog();
        return 2;
    }

    log_msg(LOG_NOTICE, "Starting sovereign network initialization");

    int hardening_fail = 0;

    hardening_fail |= set_sysctl("net.ipv4.conf.all.rp_filter", 1);
    hardening_fail |= set_sysctl("net.ipv4.conf.all.accept_redirects", 0);
    hardening_fail |= set_sysctl("net.ipv4.conf.all.secure_redirects", 0);
    hardening_fail |= set_sysctl("net.ipv4.conf.all.send_redirects", 0);
    hardening_fail |= set_sysctl("net.ipv4.conf.all.accept_source_route", 0);
    hardening_fail |= set_sysctl("net.ipv6.conf.all.accept_redirects", 0);
    hardening_fail |= set_sysctl("net.ipv6.conf.all.accept_source_route", 0);
    hardening_fail |= set_sysctl("net.ipv4.tcp_syncookies", 1);
    hardening_fail |= set_sysctl("net.ipv4.icmp_echo_ignore_broadcasts", 1);
    hardening_fail |= set_sysctl("net.ipv4.conf.all.log_martians", 1);
    hardening_fail |= set_sysctl("net.ipv4.icmp_ignore_bogus_error_responses", 1);

    if (hardening_fail) {
        log_msg(LOG_WARNING, "Some sysctl hardening values failed to apply");
    } else {
        log_msg(LOG_INFO, "Kernel networking hardening complete");
    }

    // iwd prep — only if wlan0 exists
    struct stat st;
    if (stat("/sys/class/net/wlan0", &st) == 0) {
        char *iwctl_argv[] = {"iwctl", "station", "wlan0", "scan", NULL};
        if (run_command("iwctl", iwctl_argv) == 0) {
            log_msg(LOG_INFO, "iwd scan triggered on wlan0");
        } else {
            log_msg(LOG_WARNING, "iwctl scan failed on wlan0");
        }
    } else {
        log_msg(LOG_INFO, "No wlan0 interface — skipping iwd scan");
    }

    // openresolv — ensure resolv.conf is managed
    char *resolv_argv[] = {"resolvconf", "-u", NULL};
    if (run_command("resolvconf", resolv_argv) == 0) {
        log_msg(LOG_INFO, "resolv.conf updated via openresolv");
    } else {
        log_msg(LOG_WARNING, "resolvconf -u failed — DNS may be unstable");
    }

    log_msg(LOG_NOTICE, "Sovereign network stack initialized (exit code %d)", hardening_fail ? 1 : 0);
    closelog();
    return hardening_fail ? 1 : 0;
}
