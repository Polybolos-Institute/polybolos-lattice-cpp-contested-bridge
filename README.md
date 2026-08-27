# polybolos-lattice-cpp-contested-bridge

## Status & recognition (factual)
**OASW(SO/LIC) Accelerator Event - July 2026 (GoColosseum)**  
Submission status: **Selected**. Per the portal, Selected means the submission was found **technically meritorious** and is under evaluation/consideration. 
**1BCT / 82nd Airborne - Operation Epic Fury challenge (GoColosseum)**  
Submission status: **Submitted**. Industry challenge from 1st Brigade Combat Team (1BCT), 82nd Airborne Division (CENTCOM AOR, Operation Epic Fury) for rapidly deployable commercial solutions in contested tactical environments. Polybolos portal submission: Advanced Tactical UAS, Edge Compute, Network, Inferencing Solutions.
**AFRL engagement - April 2026**  
COMMAND HOTL materials were provided to Air Force Research Laboratory contacts at their request:
- **Col Christopher Rondeau (AFRL/RQ):** after receiving the package, requested permission to share it with additional colleagues while **building out this portfolio**; permission granted (**portfolio review / distribution interest**).
- **Isaac Weintraub, PhD (Control Science Center, Air Warfare Directorate / RA):** detailed technical Q&A on risk awareness, weaponeering, kinematics, and coordination. He wrote that the exchange helped him understand **"the state of the art"** and what can be gained through **future partnerships**, and indicated he would convey **SBIR** topic materials and/or partnering.
That is attributed scientific and portfolio dialogue. 
**Technology maturity**  
Command HOTL is assessed at **TRL 5** (lab / SITL / controlled demo / Lattice developer sandbox). Decision-C2 / human-on-the-loop authority lineage. 
**Lattice**  
Sandbox / interoperability evidence (including documented scale publish-ingest work) supports Lattice-edge integration feasibility. Not a production Lattice mesh claim. Independent of Anduril; samples are not Anduril products.
**Inquiries:** mark.brown@polybolos.org  
CAGE: 1AVY9 · UEI: RUSHH9B2UQV3 · Polybolos Institute

## What this repo does

1. WinHTTP Lattice client: OAuth, entity PUT, optional SSE (`https://` Sandboxes and `http://` local mock)
2. Contested bridge: edge track coast continues while publish is denied; PUT resumes when the link returns
3. Auth-resume probe: clear cached OAuth token, re-auth, publish again
4. Known-good entity fixtures under `fixtures/`
5. Reproducible evidence: fuse timing plus ok / fail / HTTP 403 counts

Full denied-comms fusion math (EKF / UKF / Hungarian) is in sibling  
[polybolos-denied-comms-c2](https://github.com/Polybolos-Institute/polybolos-denied-comms-c2).  
This repository focuses on C++ interop and contested publish continuity.

## Quick start (offline evidence)

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\Release\contested_bridge.exe --dry-run --frames 120 --up 20 --down 10
```

Expect: `RESULT: PASS (edge tracks survived contested windows)`.

## Live probes (Sandboxes credentials via env)

```powershell
.\build\Release\lattice_probe.exe
.\build\Release\auth_resume_probe.exe
.\build\Release\contested_bridge.exe --tracks 10 --frames 150 --up 25 --down 15
```

Env: `LATTICE_ENDPOINT` `LATTICE_CLIENT_ID` `LATTICE_CLIENT_SECRET` `LATTICE_ENV_TOKEN`  
(Account Sandboxes Bearer for `LATTICE_ENV_TOKEN`. Do not commit secrets.)

## Mock Lattice (CI / local)

```powershell
# terminal A
git clone https://github.com/Polybolos-Institute/anduril-mock-lattice
cd anduril-mock-lattice
python -m mock_lattice

# terminal B
$env:LATTICE_ENDPOINT = "http://127.0.0.1:8765"
$env:LATTICE_CLIENT_ID = "test-client-id"
$env:LATTICE_CLIENT_SECRET = "test-client-secret"
$env:LATTICE_ENV_TOKEN = "test-sandbox-token"
.\build\Release\contested_bridge.exe --frames 60 --up 15 --down 10
```

Python evidence harness (no C++ required):

```bash
pip install -r tools/requirements-dev.txt
python tools/contested_evidence.py --dry-run --frames 60 --up 15 --down 10
```

## Contested schedule

Default: **20 frames link up / 10 frames link down**, repeating.

- Link **down**: tracks keep coasting on-edge; no PUT
- Link **up**: resume entity PUT to Lattice (or mock)
- Pass rule: denial frames occurred AND at least one track confirmed

CLI knobs: `--tracks N`, `--frames N`, `--up N`, `--down N`, `--prefix ID`.  
Soak example: `--tracks 10 --frames 150 --up 25 --down 15`.  
Report includes fuse_us and put_ms percentiles plus put success rate.

## Evidence

See [docs/EVIDENCE.md](docs/EVIDENCE.md).

Sibling fusion benches (denied-comms C2): ~3.6 µs/frame, 5/5 tracks confirmed on synthetic scenarios.

## Lattice doors map (Polybolos Institute)

| Door | What it proves |
|---|---|
| [polybolos-lattice-cpp-contested-bridge](https://github.com/Polybolos-Institute/polybolos-lattice-cpp-contested-bridge) | Edge continuity + contested publish (this repo) |
| [polybolos-denied-comms-c2](https://github.com/Polybolos-Institute/polybolos-denied-comms-c2) | Full edge fusion math under denial |
| [anduril-lattice-rest-winhttp](https://github.com/Polybolos-Institute/anduril-lattice-rest-winhttp) | Shared WinHTTP REST helper |
| [anduril-lattice-publish-soak](https://github.com/Polybolos-Institute/anduril-lattice-publish-soak) | Firehose PUT soak; count 403s; no throttle |
| [anduril-mock-lattice](https://github.com/Polybolos-Institute/anduril-mock-lattice) | Local OAuth + PUT stand-in for CI |
| [anduril-lattice-entity-fixtures](https://github.com/Polybolos-Institute/anduril-lattice-entity-fixtures) | Known-good entity JSON shapes |
| [anduril-lattice-stream-watcher](https://github.com/Polybolos-Institute/anduril-lattice-stream-watcher) | Entity stream / SSE sample |
| [anduril-mavlink-lattice-bridge](https://github.com/Polybolos-Institute/anduril-mavlink-lattice-bridge) | MAVLink → Lattice publish |
| [anduril-opensky-lattice-bridge](https://github.com/Polybolos-Institute/anduril-opensky-lattice-bridge) | OpenSky ADS-B → Lattice |
| [anduril-dump1090-lattice-bridge](https://github.com/Polybolos-Institute/anduril-dump1090-lattice-bridge) | dump1090 ADS-B → Lattice |
| [anduril-lattice-sandbox-dx](https://github.com/Polybolos-Institute/anduril-lattice-sandbox-dx) | Sandbox DX kit / ontology cheat sheet |

## Not in scope

- Tasks / Objects / weapons authorization
- Full Lattice World Model parity


## License

MIT. See [LICENSE](LICENSE).

## Contact

This repository is the open foundation (MIT).

Polybolos Institute also maintains a proprietary catalog of additional capabilities that are not published here. Contact us to discuss production deployment and commercial licensing.

mark.brown@polybolos.org · https://www.polybolos.org
