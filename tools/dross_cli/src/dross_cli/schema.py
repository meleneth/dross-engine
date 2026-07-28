from __future__ import annotations

from pathlib import Path
import re
from typing import Literal

from pydantic import BaseModel, ConfigDict, Field, ValidationError, field_validator
import yaml


STABLE_ID_PATTERN = re.compile(r"^[a-z][a-z0-9_]*:[a-z][a-z0-9_]*$")
IDENTIFIER_PATTERN = re.compile(r"^[A-Za-z][A-Za-z0-9]*$")


class SchemaCatalogError(ValueError):
    pass


class FieldSchema(BaseModel):
    model_config = ConfigDict(extra="forbid")

    name: str
    type: Literal["content_id", "entity_ref", "hex_pose", "hit_points", "uint32"]
    optional: bool = False

    @field_validator("name")
    @classmethod
    def validate_name(cls, value: str) -> str:
        if not re.fullmatch(r"[a-z][a-z0-9_]*", value):
            raise ValueError("field names must be lower_snake_case")
        return value


class ApiSchema(BaseModel):
    model_config = ConfigDict(extra="forbid")

    kind: Literal["command", "event", "rule"]
    namespace: str
    name: str
    version: int = Field(ge=1)
    stable_id: str = Field(alias="id")
    fields: list[FieldSchema]

    @field_validator("namespace")
    @classmethod
    def validate_namespace(cls, value: str) -> str:
        if not all(re.fullmatch(r"[a-z][a-z0-9_]*", part) for part in value.split(".")):
            raise ValueError("namespace must contain lower_snake_case segments")
        return value

    @field_validator("name")
    @classmethod
    def validate_name(cls, value: str) -> str:
        if not IDENTIFIER_PATTERN.fullmatch(value):
            raise ValueError("name must be a C++ identifier")
        return value

    @field_validator("stable_id")
    @classmethod
    def validate_stable_id(cls, value: str) -> str:
        if not STABLE_ID_PATTERN.fullmatch(value):
            raise ValueError("id must be a canonical stable ID")
        return value


def load_catalog(project_root: Path) -> list[ApiSchema]:
    schemas: list[ApiSchema] = []
    schema_root = project_root / "schemas"
    for family in ("commands", "events", "rules"):
        for path in sorted((schema_root / family).glob("*.yaml")):
            try:
                document = yaml.safe_load(path.read_text(encoding="utf-8"))
                schema = ApiSchema.model_validate(document)
            except (OSError, yaml.YAMLError, ValidationError) as error:
                raise SchemaCatalogError(f"{path}: {error}") from error
            expected_kind = family[:-1]
            if schema.kind != expected_kind:
                raise SchemaCatalogError(
                    f"{path}: kind {schema.kind!r} does not match {expected_kind!r}"
                )
            schemas.append(schema)

    schemas.sort(key=lambda schema: schema.stable_id)
    seen: set[str] = set()
    for schema in schemas:
        if schema.stable_id in seen:
            raise SchemaCatalogError(f"duplicate stable id: {schema.stable_id}")
        seen.add(schema.stable_id)
    return schemas
