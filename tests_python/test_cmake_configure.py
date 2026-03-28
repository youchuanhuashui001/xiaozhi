def test_cmake_configure_generates_cache(tmp_path):
    from pathlib import Path
    import subprocess

    repo_root = Path(__file__).resolve().parents[1]
    build_dir = tmp_path / "build-cmake"
    rc = subprocess.run(
        ["cmake", "-S", str(repo_root), "-B", str(build_dir)],
        capture_output=True,
        text=True,
    )
    assert rc.returncode == 0, rc.stderr
    assert (build_dir / "CMakeCache.txt").is_file()
