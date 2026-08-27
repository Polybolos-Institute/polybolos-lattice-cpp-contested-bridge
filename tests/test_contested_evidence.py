import json
import subprocess
import sys
from pathlib import Path


def test_dry_run_contested_evidence():
    root = Path(__file__).resolve().parents[1]
    script = root / "tools" / "contested_evidence.py"
    out = subprocess.check_output(
        [sys.executable, str(script), "--dry-run", "--frames", "30", "--up", "10", "--down", "5"],
        text=True,
    )
    assert "RESULT: PASS" in out
    # last JSON object before RESULT line
    lines = [ln for ln in out.splitlines() if ln.strip().startswith("{") or ln.strip().startswith("}")]
    # parse by finding first { to last }
    start = out.find("{")
    end = out.rfind("}")
    assert start >= 0 and end > start
    data = json.loads(out[start : end + 1])
    assert data["link_down"] > 0
    assert data["frames"] == 30
