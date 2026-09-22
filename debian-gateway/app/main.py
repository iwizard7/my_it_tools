from __future__ import annotations

import json
import os
import time
import uuid
from pathlib import Path
from typing import Any

import httpx
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field, HttpUrl

app = FastAPI(title="ESP32 IT Tools Debian Gateway", version="0.1.0")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

DATA_DIR = Path(os.getenv("GATEWAY_DATA_DIR", "./data"))
COLLECTIONS_FILE = DATA_DIR / "collections.json"
MAX_RESPONSE_BYTES = 2 * 1024 * 1024


class RequestSpec(BaseModel):
    method: str = Field(default="GET", pattern="^(GET|POST|PUT|PATCH|DELETE|HEAD|OPTIONS)$")
    url: HttpUrl
    headers: dict[str, str] = Field(default_factory=dict)
    params: dict[str, str] = Field(default_factory=dict)
    body: Any | None = None
    raw_body: str | None = None
    timeout_ms: int = Field(default=10_000, ge=100, le=120_000)
    follow_redirects: bool = True


def read_collections() -> list[dict[str, Any]]:
    if not COLLECTIONS_FILE.exists():
        return []
    return json.loads(COLLECTIONS_FILE.read_text())


def write_collections(value: list[dict[str, Any]]) -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    COLLECTIONS_FILE.write_text(json.dumps(value, indent=2))


@app.get("/healthz")
async def healthz() -> dict[str, str]:
    return {"status": "ok"}


@app.get("/version")
async def version() -> dict[str, str]:
    return {"name": "debian-gateway", "version": app.version}


@app.post("/api/request")
async def request(spec: RequestSpec) -> dict[str, Any]:
    started = time.perf_counter()
    content: bytes | None = None
    headers = dict(spec.headers)
    if spec.raw_body is not None:
        content = spec.raw_body.encode()
    elif spec.body is not None:
        content = json.dumps(spec.body).encode()
        headers.setdefault("content-type", "application/json")

    try:
        timeout = httpx.Timeout(spec.timeout_ms / 1000)
        async with httpx.AsyncClient(follow_redirects=spec.follow_redirects, timeout=timeout) as client:
            response = await client.request(
                spec.method,
                str(spec.url),
                headers=headers,
                params=spec.params,
                content=content,
            )
        body = response.content[:MAX_RESPONSE_BYTES]
        return {
            "requestId": str(uuid.uuid4()),
            "ok": response.is_success,
            "status": response.status_code,
            "reason": response.reason_phrase,
            "url": str(response.url),
            "latencyMs": round((time.perf_counter() - started) * 1000, 2),
            "size": len(response.content),
            "truncated": len(response.content) > len(body),
            "headers": dict(response.headers),
            "body": body.decode("utf-8", errors="replace"),
        }
    except httpx.HTTPError as exc:
        raise HTTPException(status_code=502, detail=str(exc)) from exc


@app.get("/api/collections")
async def collections() -> list[dict[str, Any]]:
    return read_collections()


@app.post("/api/collections")
async def save_collection(collection: dict[str, Any]) -> dict[str, Any]:
    values = read_collections()
    collection.setdefault("id", str(uuid.uuid4()))
    values = [x for x in values if x.get("id") != collection["id"]]
    values.append(collection)
    write_collections(values)
    return collection


@app.delete("/api/collections/{collection_id}")
async def delete_collection(collection_id: str) -> dict[str, bool]:
    values = [x for x in read_collections() if x.get("id") != collection_id]
    write_collections(values)
    return {"deleted": True}
