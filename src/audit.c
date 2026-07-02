/*
 * audit.c - append-only audit trail. See audit.h.
 */
#define _POSIX_C_SOURCE 200809L

#include "audit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

static FILE *audit_fp = NULL;
static char audit_agent[64] = "agent";

static const char *const MONTHS[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

int audit_init(const char *dir, const char *agent) {
    if (dir == NULL || agent == NULL) {
        return -1;
    }
    /* Best-effort create of the audit directory (ignore "already exists"). */
    mkdir(dir, 0755);

    snprintf(audit_agent, sizeof(audit_agent), "%s", agent);

    char path[512];
    snprintf(path, sizeof(path), "%s/%s.log", dir, agent);
    audit_fp = fopen(path, "a");
    if (audit_fp == NULL) {
        perror("audit: fopen");
        return -1;
    }
    return 0;
}

void audit_log(const char *command, const char *args, int allowed) {
    if (audit_fp == NULL) {
        return;
    }
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);

    /* Mon/DD/YYYY is the date shape the bash query tools parse. */
    fprintf(audit_fp, "%s/%02d/%04d %02d:%02d:%02d %s %s %s%s%s\n",
            MONTHS[tmv.tm_mon], tmv.tm_mday, tmv.tm_year + 1900,
            tmv.tm_hour, tmv.tm_min, tmv.tm_sec,
            audit_agent,
            command != NULL ? command : "",
            allowed ? "ALLOW" : "DENY",
            (args != NULL && args[0] != '\0') ? " " : "",
            (args != NULL) ? args : "");
    fflush(audit_fp);
}

void audit_close(void) {
    if (audit_fp != NULL) {
        fclose(audit_fp);
        audit_fp = NULL;
    }
}
