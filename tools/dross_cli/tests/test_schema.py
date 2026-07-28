from pathlib import Path

import pytest

from dross_cli.schema import SchemaCatalogError, load_catalog


def write_schema(root: Path, family: str, filename: str, text: str) -> None:
    destination = root / "schemas" / family / filename
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(text, encoding="utf-8")


def test_catalog_accepts_the_phase_four_field_vocabulary(tmp_path: Path) -> None:
    write_schema(
        tmp_path,
        "commands",
        "place_entity.yaml",
        """
kind: command
namespace: dross.placement
name: PlaceEntity
version: 1
id: dross:place_entity
fields:
  - name: entity
    type: entity_ref
  - name: target
    type: hex_pose
  - name: ability
    type: content_id
  - name: damage
    type: hit_points
  - name: action_points
    type: uint32
""".lstrip(),
    )

    catalog = load_catalog(tmp_path)

    assert [schema.stable_id for schema in catalog] == ["dross:place_entity"]
    assert [field.type for field in catalog[0].fields] == [
        "entity_ref",
        "hex_pose",
        "content_id",
        "hit_points",
        "uint32",
    ]


def test_catalog_rejects_an_unknown_field_type(tmp_path: Path) -> None:
    write_schema(
        tmp_path,
        "events",
        "placed.yaml",
        """
kind: event
namespace: dross.placement
name: EntityPlaced
version: 1
id: dross:entity_placed
fields:
  - name: payload
    type: dictionary
""".lstrip(),
    )

    with pytest.raises(SchemaCatalogError, match="dictionary"):
        load_catalog(tmp_path)


def test_catalog_rejects_duplicate_stable_ids(tmp_path: Path) -> None:
    for family, name in (("commands", "place.yaml"), ("events", "placed.yaml")):
        write_schema(
            tmp_path,
            family,
            name,
            f"""
kind: {"command" if family == "commands" else "event"}
namespace: dross.placement
name: {"PlaceEntity" if family == "commands" else "EntityPlaced"}
version: 1
id: dross:placement
fields:
  - name: entity
    type: entity_ref
""".lstrip(),
        )

    with pytest.raises(SchemaCatalogError, match="duplicate stable id"):
        load_catalog(tmp_path)
