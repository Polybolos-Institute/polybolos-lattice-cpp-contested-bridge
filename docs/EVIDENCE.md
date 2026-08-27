# Evidence

Snapshot date: **2026-07-29**  
Repo: `polybolos-lattice-cpp-contested-bridge`  
Independent Polybolos sample. Not an Anduril product.

## Contested bridge (dry-run)

Command:

```text
contested_bridge --dry-run --frames 120 --up 20 --down 10
```

Expected shape:

| Metric | Expected |
|---|---|
| frames | 120 |
| link_down frames | 40 (given up=20, down=10 over 120 frames) |
| confirmed tracks | >= 1 |
| RESULT | PASS |

Exact fuse_us numbers are host-dependent; they must remain well under a 50 ms real-time budget.

## Contested bridge (mock Lattice)

With [anduril-mock-lattice](https://github.com/Polybolos-Institute/anduril-mock-lattice) on `http://127.0.0.1:8765`:

```text
contested_bridge --frames 60 --up 15 --down 10
```

Expected:

| Metric | Expected |
|---|---|
| puts ok | > 0 |
| tracks survive denial | confirmed=yes after resume |
| RESULT | PASS |

Optional rate-limit drill: set `MOCK_FAIL_AFTER_N` on the mock; expect `http_403` counters to rise. This sample does not add a client-side publish throttle; HTTP 403 counts are recorded as observed.

## Live Sandboxes contested run (2026-07-29)

Redacted operator runs against a live Lattice Sandboxes environment (WinHTTP). No credentials or environment hostnames are stored in this repository.

### Smoke (3 tracks)

| Check | Result |
|---|---|
| OAuth client-credentials + Sandboxes Bearer | PASS |
| Probe entity PUT | HTTP 200 |
| Contested schedule | 30 frames, 20 up / 10 down |
| Publish | 59 ok, 1 fail (HTTP 403) |
| Tracks confirmed after denial windows | 3/3 |
| RESULT | PASS |

### Soak (10 tracks)

Command shape:

```text
contested_bridge --tracks 10 --frames 150 --up 25 --down 15 --prefix polybolos-soak-0729
```

| Metric | Result |
|---|---|
| frames | 150 (100 link_up / 50 link_down) |
| wall clock | ~171 s |
| fuse_us | p50 1.2, p95 2.0 |
| put_ms | p50 161, p95 229, mean 171 |
| publish | 998 ok, 2 fail (HTTP 403), success 99.8% |
| tracks confirmed after denial | 10/10 |
| RESULT | PASS |

Entity ID prefixes in those runs included `polybolos-probe-001`, `polybolos-contested-*`, and `polybolos-soak-0729-*`. HTTP 403s are counted; this sample does not add a client publish throttle.

### Auth resume probe (2026-07-29)

Command shape:

```text
auth_resume_probe
```

| Check | Result |
|---|---|
| FetchToken | PASS |
| PutEntity (warm token) | HTTP 200 |
| ClearAccessToken | cache invalidated |
| EnsureToken + PutEntity (re-auth) | HTTP 200 |
| Resume PutEntity after short denial pause | HTTP 200 |
| RESULT | PASS |

### Visible contested proof (Entity Explorer IDs)

```text
contested_bridge --tracks 5 --frames 60 --up 20 --down 10 --prefix polybolos-proof-0730
```

| Metric | Result |
|---|---|
| frames | 60 (40 link_up / 20 link_down) |
| publish | 200 ok / 0 fail |
| tracks confirmed | 5/5 |
| RESULT | PASS |

Look for entity IDs `polybolos-proof-0730-1..5` and `polybolos-auth-resume-001` in Entity Explorer (redacted operator environment).

## Sibling fusion receipts

From [polybolos-denied-comms-c2](https://github.com/Polybolos-Institute/polybolos-denied-comms-c2) Release benches:

| Scenario set | Measurements | Latency | Throughput | Tracks |
|---|---|---|---|---|
| 5 combat scenarios | 8,601 | 3.60 µs/frame | ~278 kHz | 5/5 confirmed |

This bridge does not re-ship the full EKF/UKF stack. It documents contested publish continuity in C++ against Lattice REST.
