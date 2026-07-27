from pathlib import Path
import subprocess


def find_project_root(start: Path | None = None) -> Path:
    current = (start or Path.cwd()).resolve()
    for candidate in (current, *current.parents):
        if (candidate / ".git").exists():
            return candidate
    raise RuntimeError("no Dross project root found (expected a parent .git)")


def require_clean_repository(project_root: Path) -> None:
    result = subprocess.run(
        ["git", "-C", project_root, "status", "--porcelain"],
        check=True,
        capture_output=True,
        text=True,
    )
    if result.stdout:
        raise RuntimeError(
            "dirty repository: commit or stash changes, or pass --allow-dirty"
        )
