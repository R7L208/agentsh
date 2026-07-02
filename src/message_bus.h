/*
 * message_bus.h - inter-agent message bus over named pipes (FIFOs).
 *
 * Agents send messages to each other through a central broker: an agent writes
 * an AgentMessage to the broker's FIFO, and the broker forwards it to the
 * target agent's FIFO. A
 * background listener thread prints incoming messages at the agent's prompt.
 */
#ifndef AGENTSH_MESSAGE_BUS_H
#define AGENTSH_MESSAGE_BUS_H

#define MESSAGE_BUS_SERVER_FIFO "serverFIFO"

typedef struct AgentMessage {
    char source[50];
    char target[50];
    char body[200];
} AgentMessage;

/*
 * Enable messaging for this agent. Returns 1 if the message bus is available
 * (the broker FIFO and this agent's own FIFO both exist) and the listener
 * thread started, 0 otherwise. When 0, message_bus_send() reports messaging off.
 */
int message_bus_start(const char *agent);

/* Send body to target via the broker. No-op with a notice if the bus is off. */
void message_bus_send(const char *target, const char *body);

/* Whether the message bus is currently enabled for this agent. */
int message_bus_enabled(void);

#endif /* AGENTSH_MESSAGE_BUS_H */
