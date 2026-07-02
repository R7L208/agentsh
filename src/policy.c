/*
 * policy.c - deny-by-default command allowlist. See policy.h.
 */
#include "policy.h"

#include <stdio.h>
#include <string.h>

/*
 * The sandbox allowlist. Every entry is handled inside agentsh against the
 * virtual filesystem or the message bus - nothing spawns a real host process,
 * so an agent can only ever touch sandboxed state. Commands absent from this
 * list (rm -rf on the host, wget, chmod, arbitrary binaries, ...) are refused.
 */
static const char *allowed[] = {
    "mkdir", "rmdir", "ls", "cd", "pwd",
    "touch", "rm", "cat", "cp", "write",
    "grep", "tree", "sendmsg", "help", "exit",
};

static const int allowed_count = (int)(sizeof(allowed) / sizeof(allowed[0]));

int policy_is_allowed(const char *cmd) {
    if (cmd == NULL) {
        return 0;
    }
    for (int i = 0; i < allowed_count; i++) {
        if (strcmp(cmd, allowed[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

void policy_print_allowed(void) {
    printf("The allowed commands are:\n");
    for (int i = 0; i < allowed_count; i++) {
        printf("%d: %s\n", i + 1, allowed[i]);
    }
}
