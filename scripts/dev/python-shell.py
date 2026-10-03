# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys


SCRIPT = Path(__file__).resolve()
ROOT = SCRIPT.parents[2]
VIRTUAL_ENVIRONMENT = ROOT / ".venv"
BUILD_DIRECTORY = ROOT / "build-python"
CONFIGURATION_STAMP = BUILD_DIRECTORY / ".pistoris-python-configured"
MINIMUM_PYTHON = (3, 12)


def fail(message: str) -> None:
    print(f"error: {message}", file=sys.stderr)
    raise SystemExit(1)


def require_supported_version(version: tuple[int, int, int], description: str) -> None:
    if version[:2] >= MINIMUM_PYTHON:
        return
    required = ".".join(map(str, MINIMUM_PYTHON))
    actual = ".".join(map(str, version))
    fail(f"{description} uses Python {actual}; Pistoris requires Python {required} or newer")


def environment_python() -> Path:
    if os.name == "nt":
        return VIRTUAL_ENVIRONMENT / "Scripts" / "python.exe"
    return VIRTUAL_ENVIRONMENT / "bin" / "python"


def read_python_version(executable: Path) -> tuple[int, int, int]:
    result = subprocess.run(
        [str(executable), "-c", "import sys; print(*sys.version_info[:3])"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        fail(f"cannot run the existing environment interpreter at {executable}; remove .venv and try again")
    try:
        major, minor, micro = (int(part) for part in result.stdout.split())
    except ValueError:
        fail(f"cannot determine the Python version for {executable}")
    return major, minor, micro


def run(command: list[str]) -> None:
    result = subprocess.run(command, cwd=ROOT, check=False)
    if result.returncode != 0:
        raise SystemExit(result.returncode)


def cmake_configuration_required() -> bool:
    cache = BUILD_DIRECTORY / "CMakeCache.txt"
    build_file = BUILD_DIRECTORY / "build.ninja"
    if not cache.is_file() or not build_file.is_file() or not CONFIGURATION_STAMP.is_file():
        return True

    configured_at = CONFIGURATION_STAMP.stat().st_mtime_ns
    inputs = (SCRIPT, ROOT / "CMakePresets.json", VIRTUAL_ENVIRONMENT / "pyvenv.cfg")
    return any(path.stat().st_mtime_ns > configured_at for path in inputs)


def main() -> int:
    require_supported_version(sys.version_info[:3], "the selected interpreter")

    if not VIRTUAL_ENVIRONMENT.exists():
        print("Creating .venv...")
        run([sys.executable, "-m", "venv", str(VIRTUAL_ENVIRONMENT)])
    elif not VIRTUAL_ENVIRONMENT.is_dir():
        fail(".venv exists but is not a directory")

    python = environment_python()
    if not python.is_file() or not (VIRTUAL_ENVIRONMENT / "pyvenv.cfg").is_file():
        fail(".venv does not contain a Python virtual environment; remove it and try again")
    require_supported_version(read_python_version(python), "the existing .venv interpreter")

    if cmake_configuration_required():
        run(
            [
                "cmake",
                "--preset",
                "python",
                f"-DPython_EXECUTABLE:FILEPATH={python.as_posix()}",
            ]
        )
        CONFIGURATION_STAMP.touch()
    run(["cmake", "--build", "--preset", "python", "--target", "pistoris_python_package"])

    environment = os.environ.copy()
    environment.pop("PYTHONHOME", None)
    environment["PYTHONNOUSERSITE"] = "1"
    environment["PYTHONPATH"] = str(BUILD_DIRECTORY / "python")
    environment["VIRTUAL_ENV"] = str(VIRTUAL_ENVIRONMENT)
    environment["PATH"] = str(python.parent) + os.pathsep + environment.get("PATH", "")
    if os.name == "nt":
        return subprocess.run([str(python)], cwd=ROOT, env=environment, check=False).returncode

    os.chdir(ROOT)
    os.execve(str(python), [str(python)], environment)


if __name__ == "__main__":
    raise SystemExit(main())
