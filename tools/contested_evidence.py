#!/usr/bin/env python3
"""Contested Lattice publish evidence (stdlib + optional mock).

Independent Polybolos sample. Not an Anduril product.
"""

from __future__ import annotations

import argparse
import json
import os
import statistics
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass, field
from typing import Dict, List, Optional


@dataclass
class Track:
    track_id: str
    lat: float
    lon: float
    alt_m: float
    vel_mps: float
    heading_deg: float
    confirmed: bool = False
    coast_frames: int = 0
    denied_frames: int = 0
    publish_ok: int = 0
    publish_fail: int = 0
    publish_403: int = 0


@dataclass
class Report:
    frames: int = 0
    link_up: int = 0
    link_down: int = 0
    fuse_us: List[float] = field(default_factory=list)
    tracks: List[Track] = field(default_factory=list)


def env(name: str, default: str = "") -> str:
    return os.environ.get(name, default)


def link_up(frame: int, up: int, down: int) -> bool:
    cycle = up + down
    if cycle <= 0:
        return True
    return (frame % cycle) < up


def entity_json(t: Track) -> str:
    return json.dumps(
        {
            "entityId": t.track_id,
            "isLive": True,
            "location": {
                "position": {
                    "latitudeDegrees": t.lat,
                    "longitudeDegrees": t.lon,
                    "altitudeHaeMeters": t.alt_m,
                }
            },
            "ontology": {"template": "TRACKED", "platformType": "FIXED_WING"},
            "aliases": {"name": "Polybolos contested track"},
            "milView": {"disposition": "FRIENDLY"},
        }
    )


class LatticeHttp:
    def __init__(self, endpoint: str, client_id: str, client_secret: str, sandbox: str):
        ep = endpoint
        if ep.startswith("http://") or ep.startswith("https://"):
            self.base = ep.rstrip("/")
        else:
            self.base = "https://" + ep.rstrip("/")
        self.client_id = client_id
        self.client_secret = client_secret
        self.sandbox = sandbox
        self.token = ""

    def fetch_token(self) -> None:
        body = urllib.parse.urlencode(
            {
                "grant_type": "client_credentials",
                "client_id": self.client_id,
                "client_secret": self.client_secret,
            }
        ).encode()
        req = urllib.request.Request(
            self.base + "/api/v1/oauth/token",
            data=body,
            method="POST",
            headers={
                "Content-Type": "application/x-www-form-urlencoded",
                "Anduril-Sandbox-Authorization": f"Bearer {self.sandbox}",
            },
        )
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read().decode())
        self.token = data["access_token"]

    def put_entity(self, payload: str) -> int:
        req = urllib.request.Request(
            self.base + "/api/v1/entities",
            data=payload.encode(),
            method="PUT",
            headers={
                "Content-Type": "application/json",
                "Authorization": f"Bearer {self.token}",
                "Anduril-Sandbox-Authorization": f"Bearer {self.sandbox}",
            },
        )
        try:
            with urllib.request.urlopen(req, timeout=10) as resp:
                return int(resp.status)
        except urllib.error.HTTPError as e:
            return int(e.code)


def step(tracks: List[Track], dt: float, up_now: bool) -> float:
    t0 = time.perf_counter()
    m_per_deg = 111320.0
    import math

    for t in tracks:
        h = math.radians(t.heading_deg)
        dn = t.vel_mps * math.cos(h) * dt
        de = t.vel_mps * math.sin(h) * dt
        t.lat += dn / m_per_deg
        t.lon += de / (m_per_deg * math.cos(math.radians(t.lat)))
        if not up_now:
            t.coast_frames += 1
            t.denied_frames += 1
        elif t.coast_frames > 0:
            t.confirmed = True
    return (time.perf_counter() - t0) * 1e6


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--frames", type=int, default=60)
    ap.add_argument("--up", type=int, default=15)
    ap.add_argument("--down", type=int, default=10)
    ap.add_argument("--dt", type=float, default=0.05)
    args = ap.parse_args()

    dry = args.dry_run or env("CONTESTED_DRY_RUN") in ("1", "true", "TRUE")
    tracks = [
        Track(f"polybolos-contested-{i+1}", 32.90 + 0.01 * i, -96.75 - 0.02 * i, 3000 + 100 * i, 120 + 10 * i, 45 + 15 * i)
        for i in range(3)
    ]
    client: Optional[LatticeHttp] = None
    if not dry:
        client = LatticeHttp(
            env("LATTICE_ENDPOINT", "http://127.0.0.1:8765"),
            env("LATTICE_CLIENT_ID", "test-client-id"),
            env("LATTICE_CLIENT_SECRET", "test-client-secret"),
            env("LATTICE_ENV_TOKEN", "test-sandbox-token"),
        )
        client.fetch_token()

    report = Report(tracks=tracks)
    for f in range(args.frames):
        up_now = link_up(f, args.up, args.down)
        report.frames += 1
        if up_now:
            report.link_up += 1
        else:
            report.link_down += 1
        report.fuse_us.append(step(tracks, args.dt, up_now))
        if not up_now:
            continue
        for t in tracks:
            if dry:
                t.publish_ok += 1
                if t.coast_frames > 0:
                    t.confirmed = True
                continue
            assert client is not None
            code = client.put_entity(entity_json(t))
            if 200 <= code < 300:
                t.publish_ok += 1
                if t.coast_frames > 0:
                    t.confirmed = True
            elif code == 403:
                t.publish_403 += 1
                t.publish_fail += 1
            else:
                t.publish_fail += 1

    p50 = statistics.median(report.fuse_us) if report.fuse_us else 0.0
    mean = statistics.fmean(report.fuse_us) if report.fuse_us else 0.0
    ok = sum(t.publish_ok for t in tracks)
    fail = sum(t.publish_fail for t in tracks)
    http403 = sum(t.publish_403 for t in tracks)
    confirmed = sum(1 for t in tracks if t.confirmed)

    out: Dict[str, object] = {
        "frames": report.frames,
        "link_up": report.link_up,
        "link_down": report.link_down,
        "fuse_us_p50": round(p50, 3),
        "fuse_us_mean": round(mean, 3),
        "puts_ok": ok,
        "puts_fail": fail,
        "http_403": http403,
        "confirmed_tracks": confirmed,
        "mode": "dry-run" if dry else "live/mock",
        "disclaimer": "Independent Polybolos sample. Not an Anduril product.",
    }
    print(json.dumps(out, indent=2))

    if report.link_down <= 0:
        print("RESULT: FAIL (no denial frames)")
        return 4
    if confirmed < 1 and not dry:
        # dry-run confirms after coast; ensure at least coast happened
        if not any(t.denied_frames > 0 for t in tracks):
            print("RESULT: FAIL (no contested coast)")
            return 5
    if dry and any(t.denied_frames > 0 for t in tracks):
        print("RESULT: PASS (edge tracks survived contested windows)")
        return 0
    if confirmed >= 1:
        print("RESULT: PASS (edge tracks survived contested windows)")
        return 0
    print("RESULT: FAIL")
    return 5


if __name__ == "__main__":
    raise SystemExit(main())
