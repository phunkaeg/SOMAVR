"""Offline controls for the apitrace depth evidence reader."""

import base64
import io
import json
from pathlib import Path
import tempfile
import unittest

import numpy as np
from PIL import Image

from analyze_hpl_depth_state import analyze, decode_image


def float_image(pixels, magic=b"Pf", scale=b"1", fmt="GL_DEPTH_COMPONENT"):
    pixels = np.asarray(pixels, dtype="<f4")
    height, width = pixels.shape[:2]
    payload = magic + b"\n" + f"{width} {height}\n".encode() + scale + b"\n" + pixels.tobytes()
    return {"__width__": width, "__height__": height, "__depth__": 1,
            "__format__": fmt, "__data__": base64.b64encode(payload).decode()}


def state_fixture(depth, linear=None):
    height, width = depth.shape
    state = {
        "parameters": {"GL_DRAW_FRAMEBUFFER_BINDING": 11,
                       "GL_READ_FRAMEBUFFER_BINDING": 11,
                       "GL_VIEWPORT": [0, 0, width, height],
                       "GL_DEPTH_TEST": "GL_TRUE", "GL_DEPTH_WRITEMASK": "GL_FALSE",
                       "GL_DEPTH_FUNC": "GL_LEQUAL"},
        "framebuffer": {"GL_DEPTH_ATTACHMENT": float_image(depth),
                        "GL_COLOR_ATTACHMENT0": float_image(np.ones((height, width, 4)), b"PX")},
        "uniforms": {}, "textures": {}, "shaders": {},
    }
    if linear is not None:
        state["uniforms"] = {"aSceneDepth": 7, "afNearPlane": .03, "afFarPlane": 1000,
                             "a_mtxProjection": [], "a_mtxView": []}
        state["textures"]["GL_TEXTURE7, GL_TEXTURE_2D, level = 0"] = float_image(linear, fmt="GL_R16F")
    return state


class DepthEvidenceTests(unittest.TestCase):
    def analyze_fixture(self, state):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.json"
            path.write_text(json.dumps(state), encoding="utf-8")
            return analyze(path, Path(directory) / "analysis")

    def test_windows_float_order_and_channels(self):
        # This exporter is top-down, native little endian even with scale +1.
        for channels, magic in ((1, b"Pf"), (3, b"PF"), (4, b"PX")):
            pixels = np.arange(2 * 3 * channels, dtype="<f4").reshape(2, 3, channels)
            for scale in (b"1", b"-1"):
                np.testing.assert_array_equal(decode_image(float_image(pixels, magic, scale)), pixels)

    def test_png_order_and_metadata(self):
        pixels = np.array([[[1, 2, 3], [4, 5, 6]], [[7, 8, 9], [10, 11, 12]]], dtype=np.uint8)
        stream = io.BytesIO()
        Image.fromarray(pixels).save(stream, format="PNG")
        item = {"__width__": 2, "__height__": 2, "__data__": base64.b64encode(stream.getvalue()).decode()}
        np.testing.assert_array_equal(decode_image(item), pixels)
        item["__width__"] = 3
        with self.assertRaises(ValueError):
            decode_image(item)

    def test_invalid_float_payloads_fail(self):
        item = float_image(np.ones((2, 3)))
        payload = base64.b64decode(item["__data__"])
        for invalid in (payload[:-4], payload + b"xxxx"):
            with self.assertRaises(ValueError):
                decode_image({**item, "__data__": base64.b64encode(invalid).decode()})
        for override in ({"__width__": 4}, {"__height__": 0}, {"__depth__": 2}):
            with self.assertRaises(ValueError):
                decode_image({**item, **override})
        with self.assertRaises(ValueError):
            decode_image(float_image(np.ones((2, 3)), scale=b"2"))

    def test_wrapped_base64_and_corruption(self):
        item = float_image(np.ones((2, 3)))
        encoded = item["__data__"]
        item["__data__"] = "\n ".join(encoded[i:i+8] for i in range(0, len(encoded), 8))
        np.testing.assert_array_equal(decode_image(item), np.ones((2, 3, 1)))
        item["__data__"] += "!"
        with self.assertRaises(ValueError):
            decode_image(item)

    def test_known_linear_depth_is_positive_control(self):
        depth = np.array([[0, .55], [.99, 1]], dtype="<f4")
        distance = .03 * 1000 / (1000 - (1000 - .03) * depth.astype(np.float64))
        linear = (distance / 1000).astype(np.float16).astype(np.float32)
        report = self.analyze_fixture(state_fixture(depth, linear))
        self.assertEqual(report["depth"]["exact_far_plane_count"], 1)
        self.assertEqual(report["scene_texture"]["within_0_2_percent_fraction"], 1)
        self.assertEqual(report["depth_write_mask"], "GL_FALSE")

    def test_wrong_texture_does_not_pass_geometry_agreement(self):
        depth = np.full((2, 2), .99, dtype="<f4")
        report = self.analyze_fixture(state_fixture(depth, np.ones((2, 2))))
        self.assertEqual(report["scene_texture"]["within_0_2_percent_fraction"], 0)

    def test_empty_default_depth_is_not_hidden_by_valid_format(self):
        report = self.analyze_fixture(state_fixture(np.ones((2, 2), dtype="<f4")))
        self.assertEqual(report["depth"]["range"], [1, 1])
        self.assertEqual(report["depth"]["exact_far_plane_count"], 4)

    def test_nonfinite_or_out_of_range_depth_fails(self):
        for bad in (np.nan, np.inf, -.1, 1.1):
            with self.assertRaises(ValueError):
                self.analyze_fixture(state_fixture(np.full((2, 2), bad)))
            with self.assertRaises(ValueError):
                self.analyze_fixture(state_fixture(np.ones((2, 2)), np.full((2, 2), bad)))


if __name__ == "__main__":
    unittest.main()
