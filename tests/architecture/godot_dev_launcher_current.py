from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys
import tempfile


def write_executable(path: Path, contents: str) -> None:
    path.write_text(contents, encoding="utf-8")
    path.chmod(0o755)


def main() -> int:
    root = Path(sys.argv[1]).resolve()
    launcher = root / "bin" / "dross-godot"
    failures: list[str] = []

    with tempfile.TemporaryDirectory(prefix="dross-godot-launcher.") as temporary:
        temporary_path = Path(temporary)
        arguments_log = temporary_path / "godot-arguments"
        build_log = temporary_path / "cmake-arguments"
        fake_godot = temporary_path / "Godot_v4.7.1-stable_linux.x86_64"
        fake_cmake = temporary_path / "cmake"

        write_executable(
            fake_godot,
            """#!/bin/sh
if [ "${1:-}" = "--version" ]; then
  echo "4.7.1.stable.official.test"
  exit 0
fi
printf '%s\\n' "$@" >"$DROSS_TEST_GODOT_ARGUMENTS"
""",
        )
        write_executable(
            fake_cmake,
            """#!/bin/sh
printf '%s\\n' "$@" >"$DROSS_TEST_CMAKE_ARGUMENTS"
""",
        )

        environment = os.environ.copy()
        environment.update(
            {
                "GODOT_BIN": str(fake_godot),
                "DROSS_TEST_GODOT_ARGUMENTS": str(arguments_log),
                "DROSS_TEST_CMAKE_ARGUMENTS": str(build_log),
                "PATH": f"{temporary_path}:{environment['PATH']}",
            }
        )
        result = subprocess.run(
            [str(launcher), "--editor", "--quit"],
            cwd=root,
            env=environment,
            text=True,
            capture_output=True,
            check=False,
        )
        if result.returncode != 0:
            failures.append(f"launcher failed: {result.stderr}")
        expected_arguments = [
            "--path",
            str(root / "godot"),
            "--editor",
            "--quit",
        ]
        if arguments_log.read_text(encoding="utf-8").splitlines() != expected_arguments:
            failures.append("launcher did not pass the project path and Godot arguments")
        expected_build = ["--build", "--preset", "linux-debug", "--target", "dross_godot"]
        if build_log.read_text(encoding="utf-8").splitlines() != expected_build:
            failures.append("launcher did not perform the incremental extension build")

        wrong_godot = temporary_path / "godot-wrong-version"
        write_executable(
            wrong_godot,
            "#!/bin/sh\necho '4.6.2.stable.official.test'\n",
        )
        environment["GODOT_BIN"] = str(wrong_godot)
        wrong_result = subprocess.run(
            [str(launcher), "--no-build"],
            cwd=root,
            env=environment,
            text=True,
            capture_output=True,
            check=False,
        )
        if wrong_result.returncode != 2 or "must be Godot 4.7.1" not in wrong_result.stderr:
            failures.append("launcher accepted an unsupported Godot version")

    if failures:
        print("\n".join(failures))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
