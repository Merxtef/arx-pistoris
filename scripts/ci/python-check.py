# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]


def run(*command: str, env: dict[str, str] | None = None) -> None:
    subprocess.run(command, cwd=ROOT, env=env, check=True)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--wheel-dir", type=Path)
    return parser.parse_args()


def main() -> None:
    arguments = parse_arguments()
    clean_environment = os.environ.copy()
    clean_environment.pop("PYTHONHOME", None)
    clean_environment.pop("PYTHONPATH", None)
    clean_environment["PYTHONNOUSERSITE"] = "1"

    with tempfile.TemporaryDirectory(prefix=".python-check-", dir=ROOT) as temporary:
        workspace = Path(temporary)
        environment = workspace / "venv"
        wheels = arguments.wheel_dir.resolve() if arguments.wheel_dir else workspace / "wheels"
        run(sys.executable, "-m", "venv", str(environment), env=clean_environment)

        python = environment / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
        if arguments.wheel_dir is None:
            run(
                str(python),
                "-m",
                "pip",
                "--disable-pip-version-check",
                "wheel",
                str(ROOT),
                "--no-deps",
                "--wheel-dir",
                str(wheels),
                env=clean_environment,
            )
        artifacts = list(wheels.glob("pistoris-*.whl"))
        if len(artifacts) != 1:
            raise RuntimeError(f"expected one pistoris wheel, found {len(artifacts)}")
        if "-cp312-abi3-" not in artifacts[0].name:
            raise RuntimeError(f"expected a cp312-abi3 wheel, found {artifacts[0].name}")

        run(
            str(python),
            "-m",
            "pip",
            "--disable-pip-version-check",
            "install",
            "--no-deps",
            str(artifacts[0]),
            env=clean_environment,
        )
        run(
            str(python),
            "-m",
            "pip",
            "--disable-pip-version-check",
            "install",
            "mypy>=1.15,<2",
            env=clean_environment,
        )

        run(
            str(python),
            "-c",
            "import pathlib, pistoris, sys; "
            "module = pathlib.Path(pistoris.__file__).resolve(); "
            "prefix = pathlib.Path(sys.prefix).resolve(); "
            "assert module.is_relative_to(prefix), f'{module} is outside {prefix}'",
            env=clean_environment,
        )
        run(
            str(python),
            "-m",
            "unittest",
            "discover",
            "-s",
            str(ROOT / "tests/python"),
            "-v",
            env=clean_environment,
        )
        run(
            str(python),
            "-m",
            "mypy",
            "--strict",
            "--no-error-summary",
            str(ROOT / "tests/python/typecheck_api.py"),
            env=clean_environment,
        )


if __name__ == "__main__":
    main()
