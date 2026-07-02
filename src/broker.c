/*
 * broker.c - the message-bus broker for agentsh.
 *
 * Reads AgentMessages from the shared broker FIFO and forwards each one to the
 * target agent's FIFO.
 *
 * Usage: create the FIFOs first, then run the broker in the background:
 *   mkfifo serverFIFO agentA agentB
 *   ./broker &
 */
#define _POSIX_C_SOURCE 200809L

#include "message_bus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

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

static void terminate(int sig) {
    (void)sig;
    printf("broker: exiting\n");
    fflush(stdout);
    exit(0);
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, terminate);

    int server = open(MESSAGE_BUS_SERVER_FIFO, O_RDONLY);
    if (server < 0) {
        perror("broker: open serverFIFO (did you mkfifo serverFIFO?)");
        return 1;
    }
    /* Keep a writer open so read() blocks instead of spinning on EOF. */
    int dummy = open(MESSAGE_BUS_SERVER_FIFO, O_WRONLY);

    printf("broker: listening on %s\n", MESSAGE_BUS_SERVER_FIFO);
    fflush(stdout);

    while (1) {
        AgentMessage req;
        int r = read_full(server, &req, sizeof(req));
        if (r < 0) {
            perror("broker: read");
            continue;
        }
        if (r == 0) {
            continue;
        }

        printf("broker: routing message from %s to %s\n", req.source, req.target);
        fflush(stdout);

        int target = open(req.target, O_WRONLY);
        if (target < 0) {
            perror("broker: open target fifo");
            continue;
        }
        if (write_full(target, &req, sizeof(req)) != 0) {
            perror("broker: write target");
        }
        close(target);
    }

    close(server);
    if (dummy >= 0) {
        close(dummy);
    }
    return 0;
}
