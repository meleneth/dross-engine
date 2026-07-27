from pathlib import Path
import subprocess

from typer.testing import CliRunner

from dross_cli.app import app


runner = CliRunner()


def init_repository(root: Path) -> None:
    subprocess.run(["git", "init", "-q", root], check=True)
    subprocess.run(
        ["git", "-C", root, "config", "user.email", "test@example.invalid"],
        check=True,
    )
    subprocess.run(
        ["git", "-C", root, "config", "user.name", "Dross Test"],
        check=True,
    )
    marker = root / "README.md"
    marker.write_text("fixture\n", encoding="utf-8")
    subprocess.run(["git", "-C", root, "add", "README.md"], check=True)
    subprocess.run(
        ["git", "-C", root, "commit", "-q", "-m", "fixture"], check=True
    )


def test_interactive_command_scaffold_refuses_a_dirty_repository(
    tmp_path: Path, monkeypatch
) -> None:
    init_repository(tmp_path)
    (tmp_path / "dirty.txt").write_text("not committed\n", encoding="utf-8")
    monkeypatch.chdir(tmp_path)

    result = runner.invoke(
        app,
        [
            "generate",
            "command",
            "placement",
            "PlaceEntity",
            "entity:entity_ref",
            "target:hex_pose",
        ],
    )

    assert result.exit_code != 0
    assert "dirty repository" in result.output.lower()
    assert not (tmp_path / "schemas" / "commands" / "place_entity.yaml").exists()
