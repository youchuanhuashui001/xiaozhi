def test_cmake_configure_generates_cache(tmp_path):
    import subprocess

    build_dir = tmp_path / "build-cmake"
    rc = subprocess.run(
        ["cmake", "-S", ".", "-B", str(build_dir)],
        capture_output=True,
        text=True,
    )
    assert rc.returncode == 0, rc.stderr
