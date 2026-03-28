def test_cmake_has_daemon_target(tmp_path):
    import subprocess
    from pathlib import Path

    repo_root = Path(__file__).resolve().parents[1]
    b = tmp_path / "b"
    subprocess.check_call(["cmake", "-S", ".", "-B", str(b)], cwd=repo_root)
    out = subprocess.check_output(["cmake", "--build", str(b), "--target", "help"], text=True)
    assert "xiaozhi_daemon" in out
    assert "xiaozhi_core" in out

    build = subprocess.run(
        ["cmake", "--build", str(b), "--target", "xiaozhi_daemon"],
        capture_output=True,
        text=True,
    )
    assert build.returncode == 0, build.stderr
