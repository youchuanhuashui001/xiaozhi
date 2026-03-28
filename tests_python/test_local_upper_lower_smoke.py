import subprocess
from pathlib import Path


def test_upper_lower_local_smoke():
    repo_root = Path(__file__).resolve().parents[1]

    proc = subprocess.run(
        ["make", "test", "TEST=test_daemon_runtime"],
        cwd=repo_root,
        capture_output=True,
        text=True,
        check=False,
    )

    output = proc.stdout + proc.stderr
    assert proc.returncode == 0, output
    assert '"name":"state_changed"' in output
