# agentsh — a from-scratch secure sandbox for agent commands

[![CI](https://github.com/R7L208/agentsh/actions/workflows/ci.yml/badge.svg)](https://github.com/R7L208/agentsh/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)

`agentsh` is a small, dependency-free **sandbox for running untrusted / AI-agent
commands**, written in C. An agent gets an interactive shell, but it can only run
allowlisted commands, its file operations happen inside an **in-memory virtual
filesystem** (never the host disk), it can coordinate with other agents over a
**message bus**, and **every action is logged** to a queryable audit trail.

It is a deny-by-default execution environment: nothing runs unless it is
explicitly permitted, nothing touches the real machine, and everything is
observable after the fact.

```
agentsh(alice)> mkdir /workspace
MKDIR SUCCESS: node /workspace successfully created
agentsh(alice)> write /workspace/report.txt quarterly numbers look good
WRITE SUCCESS: wrote 27 bytes to /workspace/report.txt
agentsh(alice)> sendmsg bob hello bob, the report is ready
agentsh(alice)> wget http://malware.example/x
NOT ALLOWED!
```

## Why this exists

Running commands emitted by an autonomous agent (or any untrusted source) safely
means answering three questions: *what is it allowed to do, where can it do it,
and how do we know what it did?* `agentsh` answers all three with four small,
composable layers:

| Layer | Guarantee | Module |
|---|---|---|
| **Policy engine** | Deny-by-default — only allowlisted commands run | `src/policy.c` |
| **Virtual filesystem** | File ops stay in-memory; the host disk is never touched | `src/virtual_filesystem.c` |
| **Message bus** | Agents coordinate through a broker, not the OS | `src/message_bus.c`, `src/broker.c` |
| **Audit trail** | Every attempt (allow *and* deny) is logged and queryable | `src/audit.c`, `tools/` |

See [`docs/architecture.md`](docs/architecture.md) for the full design.

## Quick start

```bash
make            # builds ./agentsh and ./broker (needs a C11 compiler)
make test       # runs the VFS / policy / isolation unit tests
make demo       # runs the full two-agent end-to-end demo
```

### Run a single-agent sandbox

```bash
./agentsh alice
agentsh(alice)> mkdir /projects
agentsh(alice)> cd /projects
agentsh(alice)> touch notes.txt
agentsh(alice)> write notes.txt remember to ship
agentsh(alice)> cat notes.txt
agentsh(alice)> tree
agentsh(alice)> exit
```

### Run multiple agents that talk to each other

```bash
# terminal 0: create the FIFOs and start the broker
mkfifo serverFIFO alice bob
./broker &

# terminal 1
./agentsh alice
agentsh(alice)> sendmsg bob build is green

# terminal 2
./agentsh bob
# ...
Incoming message from alice: build is green
```

### Query the audit trail

Every session appends to `audit/<agent>.log` in a query-friendly format:

```
Jul/02/2026 16:54:37 alice mkdir ALLOW /workspace
Jul/02/2026 16:54:37 alice wget  DENY  http://malware.example/x
```

```bash
./tools/logquery.sh audit alice Jul   # all of alice's actions in July
                                       # -> queryResults/alice_Jul_2026.log
./tools/usage.sh DENY audit            # how many commands were denied
```

## Allowed commands

`mkdir`, `rmdir`, `ls`, `cd`, `pwd`, `touch`, `rm`, `cat`, `cp`, `write`,
`grep`, `tree`, `sendmsg`, `help`, `exit`. Anything else is refused with
`NOT ALLOWED!`. The allowlist lives in `src/policy.c`.

## Project layout

```
agentsh/
├── src/
│   ├── agentsh.c              # REPL + dispatcher (single entrypoint)
│   ├── policy.{c,h}           # deny-by-default allowlist
│   ├── virtual_filesystem.{c,h}  # in-memory FS tree + commands
│   ├── message_bus.{c,h}      # sendmsg + listener thread
│   ├── broker.c              # message router between agents
│   └── audit.{c,h}           # append-only audit trail
├── tools/
│   ├── logquery.sh           # query audit log by agent + month
│   └── usage.sh              # count keyword occurrences
├── tests/
│   ├── run_tests.sh          # VFS / policy / isolation tests
│   └── demo.sh               # end-to-end two-agent demo
├── docs/architecture.md
├── Makefile
└── LICENSE                   # Apache 2.0
```

## Roadmap / possible extensions

The current build is a complete, runnable system. Natural next steps that would
deepen it:

- **Config-driven policy** — load the allowlist from a `policy.conf` file (and
  per-agent policies) instead of hardcoding it in `policy.c`, so the sandbox can
  be re-scoped without recompiling.
- **Structured JSON audit logs** — emit newline-delimited JSON alongside the
  human/bash-queryable text format, for ingestion into log pipelines.
- **CI + containerized demo** — a GitHub Actions workflow that runs `make test`
  on every push, plus a `Dockerfile` so `docker run agentsh` gives a reproducible
  sandbox.
- **Resource limits & quotas** — cap VFS node count / total bytes per agent, and
  add a per-session command budget, to model denial-of-service protection.
- **Richer VFS semantics** — file permissions/ownership per agent, recursive
  `rm -r` and `cp -r`, and glob support.
- **asciinema recording** — an embedded terminal cast of the demo in this README.

## License

Licensed under the Apache License, Version 2.0. See [`LICENSE`](LICENSE).
