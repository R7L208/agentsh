/*
 * audit.h - append-only audit trail for the agentsh sandbox.
 *
 * Every command an agent attempts is logged in a line format that the bash
 * query tools (tools/logquery.sh, tools/usage.sh) can parse:
 *
 *   Mon/DD/YYYY HH:MM:SS <agent> <command> <ALLOW|DENY> <args...>
 *
 * The leading Mon/DD/YYYY token keeps audit logs queryable out of the box by
 * the bash query tools.
 */
#ifndef AGENTSH_AUDIT_H
#define AGENTSH_AUDIT_H

/*
 * Open (creating if needed) the audit log for this agent. The log path is
 *   <dir>/<agent>.log
 * Returns 0 on success, -1 on failure. Safe to call once at startup.
 */
int audit_init(const char *dir, const char *agent);

/* Append one entry. allowed != 0 records ALLOW, otherwise DENY. */
void audit_log(const char *command, const char *args, int allowed);

/* Flush and close the audit log. */
void audit_close(void);

#endif /* AGENTSH_AUDIT_H */
