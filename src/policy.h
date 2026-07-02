/*
 * policy.h - deny-by-default command allowlist for the agentsh sandbox.
 *
 * Any command not on the allowlist is refused before it can execute.
 */
#ifndef AGENTSH_POLICY_H
#define AGENTSH_POLICY_H

/* Returns 1 if cmd is permitted to run in the sandbox, 0 otherwise. */
int policy_is_allowed(const char *cmd);

/* Prints the allowlist (used by the built-in "help" command). */
void policy_print_allowed(void);

#endif /* AGENTSH_POLICY_H */
