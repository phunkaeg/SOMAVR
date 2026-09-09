"""Inspect complete Windows apitrace GL state dumps, not MCP image summaries.

No game/process access. Float images in this Windows exporter use native little
endian payloads, including when their PFM-like scale header is positive.
"""

import argparse
import base64
import hashlib
import io
import json
from pathlib import Path

import numpy as np
from PIL import Image


def decode_image(value):
    raw = base64.b64decode("".join(value["__data__"].split()), validate=True)
    if value["__width__"] <= 0 or value["__height__"] <= 0 or value.get("__depth__", 1) != 1:
        raise ValueError("Expected a nonempty 2D image")
    if raw.startswith(b"\x89PNG\r\n\x1a\n"):
        with Image.open(io.BytesIO(raw)) as image:
            if image.size != (value["__width__"], value["__height__"]):
                raise ValueError("PNG dimensions disagree with image metadata")
            return np.array(image.convert("RGB"))
    magic, dimensions, scale, payload = raw.split(b"\n", 3)
    width, height = map(int, dimensions.split())
    channels = {b"Pf": 1, b"PF": 3, b"PX": 4, b"P5": 1, b"P6": 3}[magic]
    dtype = "u1" if magic in (b"P5", b"P6") else "<f4"
    if (width, height) != (value["__width__"], value["__height__"]):
        raise ValueError("Payload dimensions disagree with image metadata")
    if dtype == "<f4" and abs(float(scale)) != 1:
        raise ValueError("Unverified float exporter scale")
    if dtype == "u1" and int(scale) != 255:
        raise ValueError("Unverified byte exporter scale")
    pixels = np.frombuffer(payload, dtype=dtype)
    if pixels.size != width * height * channels:
        raise ValueError("Incomplete image payload")
    return pixels.reshape(height, width, channels)


def statistics(data):
    finite = np.isfinite(data)
    values = data[finite]
    if values.size == 0:
        raise ValueError("No finite depth samples")
    h, w = data.shape
    return {
        "pixel_count": int(data.size),
        "finite_count": int(values.size),
        "range": [float(values.min()), float(values.max())],
        "quantiles_0_1_50_99_100": np.quantile(values, [0, .01, .5, .99, 1]).tolist(),
        "exact_far_plane_count": int(np.count_nonzero(data == 1)),
        "centre": float(data[h // 2, w // 2]),
    }


def analyze(path, output):
    with path.open(encoding="utf-8") as stream:
        # apitrace embeds shader source with literal control characters.
        state = json.load(stream, strict=False)
    framebuffer = state["framebuffer"]
    depth_key = next(k for k in framebuffer if k in ("GL_DEPTH_ATTACHMENT", "GL_DEPTH_COMPONENT"))
    depth = decode_image(framebuffer[depth_key])[:, :, 0]
    if not np.issubdtype(depth.dtype, np.floating) or not np.isfinite(depth).all() or np.any(depth < 0) or np.any(depth > 1):
        raise ValueError("Depth is not finite normalized GL depth")
    output.mkdir(parents=True, exist_ok=True)
    prefix = output / path.stem
    np.save(str(prefix) + "-depth.npy", depth)
    with path.open("rb") as stream:
        state_hash = hashlib.file_digest(stream, "sha256").hexdigest()
    report = {
        "state_file": str(path.resolve()),
        "state_sha256": state_hash,
        "draw_fbo": state["parameters"]["GL_DRAW_FRAMEBUFFER_BINDING"],
        "read_fbo": state["parameters"]["GL_READ_FRAMEBUFFER_BINDING"],
        "viewport": state["parameters"]["GL_VIEWPORT"],
        "depth_format": framebuffer[depth_key]["__format__"],
        "depth_test": state["parameters"].get("GL_DEPTH_TEST"),
        "depth_write_mask": state["parameters"].get("GL_DEPTH_WRITEMASK"),
        "depth_function": state["parameters"].get("GL_DEPTH_FUNC"),
        "depth": statistics(depth),
        "scope": "existing trace GPU replay; not current mod or headset acceptance",
    }
    uniforms = state["uniforms"]
    if "aSceneDepth" in uniforms and "afFarPlane" in uniforms:
        unit = int(uniforms["aSceneDepth"])
        texture_key = f"GL_TEXTURE{unit}, GL_TEXTURE_2D, level = 0"
        texture = state["textures"][texture_key]
        linear = decode_image(texture)[:, :, 0]
        if not np.isfinite(linear).all() or np.any(linear < 0) or np.any(linear > 1):
            raise ValueError("Sampled scene depth is not finite normalized linear depth")
        near, far = float(uniforms["afNearPlane"]), float(uniforms["afFarPlane"])
        if not 0 < near < far or linear.shape != depth.shape:
            raise ValueError("Invalid projection or mismatched scene-depth dimensions")
        z = near * far / (far - (far - near) * depth.astype(np.float64))
        residual = np.abs(z / far - linear)
        relative = residual / np.maximum(np.abs(linear), 1e-9)
        report["scene_texture"] = {
            "unit": unit, "format": texture["__format__"],
            "near": near, "far": far,
            "statistics": statistics(linear),
            "relative_error_max": float(relative.max()),
            "within_0_2_percent_fraction": float(np.mean(relative < .002)),
            "absolute_normalized_error_max": float(residual.max()),
            "centre_distance_world_units": float(z[z.shape[0] // 2, z.shape[1] // 2]),
            "projection_column_arrays": uniforms["a_mtxProjection"],
            "view_column_arrays": uniforms["a_mtxView"],
        }
        np.save(str(prefix) + "-linear-depth.npy", linear)
        # Scientific preview: near is bright; the 99th percentile is black.
        limit = float(np.quantile(z, .99))
        preview = np.uint8(np.clip(1 - z / limit, 0, 1) * 255)
        Image.fromarray(preview).save(str(prefix) + "-depth-preview.png")
    else:
        Image.fromarray(np.uint8((1 - depth) * 255)).save(
            str(prefix) + "-depth-preview.png")
    color_key = next(k for k in framebuffer if k in ("GL_COLOR_ATTACHMENT0", "GL_BACK"))
    color = decode_image(framebuffer[color_key])[:, :, :3]
    if color.dtype != np.uint8:
        # Preview only; original HDR values remain in the complete state dump.
        color = np.uint8(np.power(np.clip(color, 0, 1), 1 / 2.2) * 255)
    Image.fromarray(color).save(str(prefix) + "-color.png")
    for kind in ("GL_FRAGMENT_SHADER", "GL_VERTEX_SHADER"):
        if kind in state["shaders"]:
            (output / f"{path.stem}-{kind}.glsl").write_text(state["shaders"][kind], encoding="utf-8")
    (output / f"{path.stem}-summary.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("states", nargs="+", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    for path in args.states:
        print(json.dumps(analyze(path, args.output), indent=2))


if __name__ == "__main__":
    main()
