from __future__ import annotations

from pathlib import Path
import re

import typer
import yaml

from dross_cli.generator import generate_all, generated_is_current
from dross_cli.project import find_project_root, require_clean_repository
from dross_cli.schema import FieldSchema


app = typer.Typer(no_args_is_help=True)
generate_app = typer.Typer(no_args_is_help=True)
check_app = typer.Typer(no_args_is_help=True)
app.add_typer(generate_app, name="generate")
app.add_typer(check_app, name="check")


@generate_app.command("all")
def generate_all_command() -> None:
    root = find_project_root()
    generate_all(root)
    typer.echo("Generated command and event APIs.")


@check_app.command("generated")
def check_generated_command() -> None:
    root = find_project_root()
    current, differences = generated_is_current(root)
    if not current:
        typer.echo("Generated output is stale: " + ", ".join(differences), err=True)
        raise typer.Exit(1)
    typer.echo("Generated output is current.")


@generate_app.command("command")
def generate_command(
    capability: str,
    name: str,
    fields: list[str] = typer.Argument(...),
    allow_dirty: bool = typer.Option(False, "--allow-dirty"),
) -> None:
    root = find_project_root()
    if not allow_dirty:
        try:
            require_clean_repository(root)
        except RuntimeError as error:
            typer.echo(str(error), err=True)
            raise typer.Exit(1) from error

    parsed_fields: list[dict[str, object]] = []
    for declaration in fields:
        try:
            field_name, field_type = declaration.split(":", maxsplit=1)
            parsed = FieldSchema(name=field_name, type=field_type)
        except (ValueError, TypeError) as error:
            typer.echo(f"invalid field {declaration!r}: {error}", err=True)
            raise typer.Exit(2) from error
        parsed_fields.append(parsed.model_dump())

    snake_name = re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()
    destination = root / "schemas" / "commands" / f"{snake_name}.yaml"
    if destination.exists():
        typer.echo(f"refusing to overwrite {destination}", err=True)
        raise typer.Exit(1)
    document = {
        "kind": "command",
        "namespace": f"dross.{capability}",
        "name": name,
        "version": 1,
        "id": f"dross:{snake_name}",
        "fields": parsed_fields,
    }
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(
        yaml.safe_dump(document, sort_keys=False),
        encoding="utf-8",
        newline="\n",
    )
    typer.echo(f"Created {destination.relative_to(root)}")


def main() -> None:
    app()


if __name__ == "__main__":
    main()
