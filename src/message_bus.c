/*
 * message_bus.c - inter-agent message bus over FIFOs. See message_bus.h.
 */
#define _POSIX_C_SOURCE 200809L

#include "message_bus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

static char bus_agent[50];
static int bus_on = 0;

/* Read exactly n bytes into buf, handling short reads. Returns 0 on EOF, -1 on
 * error, 1 on a full read. */
static int read_full(int fd, void *buf, size_t n) {
    size_t off = 0;
    while (off < n) {
        ssize_t r = read(fd, (char *)buf + off, n - off);
        if (r < 0) {
            return -1;
        }
        if (r == 0) {
            return 0;
        }
        off += (size_t)r;
    }
    return 1;
}

/* Write exactly n bytes from buf, handling short writes. Returns 0/-1. */
static int write_full(int fd, const void *buf, size_t n) {
    size_t off = 0;
    while (off < n) {
        ssize_t w = write(fd, (const char *)buf + off, n - off);
        if (w < 0) {
            return -1;
        }
        off += (size_t)w;
    }
    return 0;
}

static void *listener(void *arg) {
    (void)arg;
    int fd = open(bus_agent, O_RDONLY);
    if (fd < 0) {
        perror("message_bus: open own fifo");
        return NULL;
    }
    while (1) {
        AgentMessage m;
        int r = read_full(fd, &m, sizeof(m));
        if (r < 0) {
            perror("message_bus: read");
            break;
        }
        if (r == 0) {
            /* All writers closed; reopen so the next send is delivered. */
            close(fd);
            fd = open(bus_agent, O_RDONLY);
            if (fd < 0) {
                perror("message_bus: reopen");
                break;
            }
            continue;
        }
        printf("\nIncoming message from %s: %s\n", m.source, m.body);
        fflush(stdout);
    }
    if (fd >= 0) {
        close(fd);
    }
    return NULL;
}

int message_bus_start(const char *agent) {
    if (agent == NULL) {
        return 0;
    }
    snprintf(bus_agent, sizeof(bus_agent), "%s", agent);

    /* Messaging is only available when both the broker FIFO and this agent's
     * own FIFO already exist (created by the demo / operator via mkfifo). */
    if (access(MESSAGE_BUS_SERVER_FIFO, F_OK) != 0 || access(bus_agent, F_OK) != 0) {
        bus_on = 0;
        return 0;
    }

    pthread_t tid;
    if (pthread_create(&tid, NULL, listener, NULL) != 0) {
        perror("message_bus: pthread_create");
        bus_on = 0;
        return 0;
    }
    pthread_detach(tid);
    bus_on = 1;
    return 1;
}

void message_bus_send(const char *target, const char *body) {
    if (!bus_on) {
        printf("sendmsg: message bus is not enabled for this agent\n");
        return;
    }
    AgentMessage m;
    memset(&m, 0, sizeof(m));
    snprintf(m.source, sizeof(m.source), "%s", bus_agent);
    snprintf(m.target, sizeof(m.target), "%s", target);
    snprintf(m.body, sizeof(m.body), "%s", body);

    int fd = open(MESSAGE_BUS_SERVER_FIFO, O_WRONLY);
    if (fd < 0) {
        perror("sendmsg: open serverFIFO");
        return;
    }
    if (write_full(fd, &m, sizeof(m)) != 0) {
        perror("sendmsg: write");
    }
    close(fd);
}

int message_bus_enabled(void) {
    return bus_on;
}
