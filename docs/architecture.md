# agentsh architecture

agentsh is a deny-by-default sandbox for running untrusted / AI-agent commands.
It is built from four independent C modules plus two bash query tools, wired
together so an agent can only ever touch sandboxed state — never the host.

## Layers

```
                        ┌─────────────────────────────┐
   agent input  ───────▶│   agentsh REPL (agentsh.c)  │
   "mkdir /x"           │   tokenizes + dispatches    │
                        └──────────────┬──────────────┘
                                       │
                        1. policy gate │  policy.c
                        (deny-by-default allowlist)
                                       │
              ┌────────────────────────┼────────────────────────┐
              │                        │                         │
              ▼                        ▼                         ▼
   2. virtual filesystem     3. message bus            (denied → "NOT ALLOWED!")
      virtual_filesystem.c      message_bus.c
      in-memory tree,           sendmsg over FIFOs
      never touches host        via the broker
              │                        │
              └───────────┬────────────┘
                          ▼
              4. audit trail  audit.c
              appends every attempt (ALLOW/DENY)
                          │
                          ▼
              tools/logquery.sh   tools/usage.sh
              query by agent+month   count keywords
```

## Modules

| Module | Responsibility |
|---|---|
| `agentsh.c` | REPL, tokenizer, command dispatch, single entrypoint |
| `policy.c` | Deny-by-default command allowlist |
| `virtual_filesystem.c` | In-memory FS tree; `mkdir`/`ls`/`cd`/`pwd`/`touch`/`rm`/`rmdir`/`cat`/`cp`/`write`/`grep`/`tree` |
| `message_bus.c` | `sendmsg` + background listener thread |
| `broker.c` | Routes messages between agents over FIFOs |
| `audit.c` | Append-only, query-friendly audit log |
| `tools/logquery.sh` | Query audit log by agent + month |
| `tools/usage.sh` | Count keyword occurrences in the audit log |

## Why it is a sandbox, not three programs

The load-bearing design decision is in `agentsh.c`'s dispatcher: filesystem
commands are **routed to `virtual_filesystem.c`** rather than spawned as host
processes. A real restricted shell that `posix_spawnp`s `mkdir` would still
mutate the host disk; agentsh's `mkdir` only mutates an in-memory tree. Combined
with the policy allowlist (nothing outside the list runs) and the audit trail
(everything is recorded), an agent is confined to sandboxed state that is fully
observable after the fact.

## Message flow (sub-agent chat)

1. Agent `alice` runs `sendmsg bob hello` → `message_bus.c` writes an
   `AgentMessage` to `serverFIFO`.
2. `broker` reads `serverFIFO`, opens `bob` (bob's FIFO), writes the message.
3. `bob`'s listener thread reads its FIFO and prints
   `Incoming message from alice: hello`.

FIFOs are created out-of-band (`mkfifo serverFIFO alice bob`); messaging is
optional — agentsh runs standalone as a single-agent sandbox when no FIFOs exist.
