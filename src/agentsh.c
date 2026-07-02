/*
 * agentsh.c - the agentsh sandbox shell (REPL + dispatcher).
 *
 * agentsh is a deny-by-default sandbox for running untrusted / AI-agent
 * commands. It combines four layers:
 *
 *   policy.c            - allowlist: only permitted commands run at all
 *   virtual_filesystem.c- file commands act on an in-memory tree, never the host
 *   message_bus.c       - agents coordinate through a FIFO broker (sub-agent chat)
 *   audit.c             - every attempt is logged for the bash query tools
 *
 * The dispatch routes filesystem commands to the virtual filesystem instead of
 * spawning host processes, which is what makes this a sandbox.
 *
 * Usage: ./agentsh [agent-name]        (default agent name: "agent")
 */
#define _POSIX_C_SOURCE 200809L

#include "policy.h"
#include "virtual_filesystem.h"
#include "message_bus.h"
#include "audit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define LINE_MAX_LEN 512
#define MAX_ARGS 32
#define AUDIT_DIR "audit"

static void terminate(int sig) {
    (void)sig;
    audit_close();
    vfs_free();
    printf("\nagentsh: exiting\n");
    fflush(stdout);
    exit(0);
}

/* Join argv[from..argc) with single spaces into dst. */
static void join_args(char *dst, size_t dstsz, char **argv, int from, int argc) {
    dst[0] = '\0';
    size_t used = 0;
    for (int i = from; i < argc; i++) {
        int n = snprintf(dst + used, dstsz - used, "%s%s",
                         (i > from) ? " " : "", argv[i]);
        if (n < 0 || (size_t)n >= dstsz - used) {
            break;
        }
        used += (size_t)n;
    }
}

int main(int argc, char **argv) {
    const char *agent = (argc >= 2) ? argv[1] : "agent";

    signal(SIGINT, terminate);

    vfs_init();
    if (audit_init(AUDIT_DIR, agent) != 0) {
        fprintf(stderr, "agentsh: warning: audit log disabled\n");
    }
    message_bus_start(agent); /* messaging is optional; VFS works standalone */

    char line[LINE_MAX_LEN];

    while (1) {
        fprintf(stderr, "agentsh(%s)> ", agent);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        if (line[0] == '\0') {
            continue;
        }

        /* Keep an untokenized copy for commands whose tail is free-form text. */
        char raw[LINE_MAX_LEN];
        snprintf(raw, sizeof(raw), "%s", line);

        char *cmdargv[MAX_ARGS + 1];
        int cmdargc = 0;
        for (char *tok = strtok(line, " \t");
             tok != NULL && cmdargc < MAX_ARGS;
             tok = strtok(NULL, " \t")) {
            cmdargv[cmdargc++] = tok;
        }
        cmdargv[cmdargc] = NULL;
        if (cmdargc == 0) {
            continue;
        }

        const char *cmd = cmdargv[0];

        /* ---- policy gate: deny-by-default ---- */
        if (!policy_is_allowed(cmd)) {
            char args[LINE_MAX_LEN];
            join_args(args, sizeof(args), cmdargv, 1, cmdargc);
            audit_log(cmd, args, 0);
            printf("NOT ALLOWED!\n");
            continue;
        }

        /* Audit the allowed attempt (args joined for the trail). */
        {
            char args[LINE_MAX_LEN];
            join_args(args, sizeof(args), cmdargv, 1, cmdargc);
            audit_log(cmd, args, 1);
        }

        /* ---- dispatch ---- */
        if (strcmp(cmd, "exit") == 0) {
            break;
        } else if (strcmp(cmd, "help") == 0) {
            policy_print_allowed();
        } else if (strcmp(cmd, "pwd") == 0) {
            vfs_pwd();
        } else if (strcmp(cmd, "tree") == 0) {
            vfs_tree();
        } else if (strcmp(cmd, "ls") == 0) {
            vfs_ls(cmdargc >= 2 ? cmdargv[1] : NULL);
        } else if (strcmp(cmd, "cd") == 0) {
            if (cmdargc > 2) {
                printf("-agentsh: cd: too many arguments\n");
            } else {
                vfs_cd(cmdargc >= 2 ? cmdargv[1] : NULL);
            }
        } else if (strcmp(cmd, "mkdir") == 0) {
            vfs_mkdir(cmdargc >= 2 ? cmdargv[1] : "/");
        } else if (strcmp(cmd, "rmdir") == 0) {
            vfs_rmdir(cmdargc >= 2 ? cmdargv[1] : "/");
        } else if (strcmp(cmd, "touch") == 0) {
            vfs_touch(cmdargc >= 2 ? cmdargv[1] : "");
        } else if (strcmp(cmd, "rm") == 0) {
            vfs_rm(cmdargc >= 2 ? cmdargv[1] : "");
        } else if (strcmp(cmd, "cat") == 0) {
            vfs_cat(cmdargc >= 2 ? cmdargv[1] : "");
        } else if (strcmp(cmd, "cp") == 0) {
            vfs_cp(cmdargc >= 2 ? cmdargv[1] : NULL,
                   cmdargc >= 3 ? cmdargv[2] : NULL);
        } else if (strcmp(cmd, "grep") == 0) {
            vfs_grep(cmdargc >= 2 ? cmdargv[1] : NULL,
                     cmdargc >= 3 ? cmdargv[2] : NULL);
        } else if (strcmp(cmd, "write") == 0) {
            if (cmdargc < 2) {
                printf("WRITE ERROR: usage: write <file> <text...>\n");
            } else {
                /* Recover the free-form text tail from the untokenized line. */
                char *text = raw + strlen("write ");
                text += strlen(cmdargv[1]);
                while (*text == ' ') {
                    text++;
                }
                vfs_write(cmdargv[1], text);
            }
        } else if (strcmp(cmd, "sendmsg") == 0) {
            if (cmdargc < 2) {
                printf("sendmsg: you have to specify target agent\n");
            } else if (cmdargc < 3) {
                printf("sendmsg: you have to enter a message\n");
            } else {
                /* target is cmdargv[1]; message is the rest of the raw line. */
                char *msg = raw + strlen("sendmsg ");
                msg += strlen(cmdargv[1]);
                while (*msg == ' ') {
                    msg++;
                }
                message_bus_send(cmdargv[1], msg);
            }
        }
    }

    audit_close();
    vfs_free();
    return 0;
}
