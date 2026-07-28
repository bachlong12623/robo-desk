import hashlib
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))

import preview_faces  # noqa: E402
import preview_screens  # noqa: E402


class PreviewRenderTests(unittest.TestCase):
    def test_face_renderer_outputs_nonempty_frame(self):
        canvas = preview_faces.render_mood("plead", 7)
        digest = hashlib.sha256(bytes(canvas.px)).hexdigest()
        self.assertEqual(len(canvas.px), preview_faces.W * preview_faces.H * 3)
        self.assertNotEqual(digest, hashlib.sha256(bytes([0]) * len(canvas.px)).hexdigest())

    def test_screen_renderer_outputs_nonempty_frame(self):
        canvas = preview_screens.render_screen("weather", 11)
        digest = hashlib.sha256(bytes(canvas.px)).hexdigest()
        self.assertEqual(len(canvas.px), preview_faces.W * preview_faces.H * 3)
        self.assertNotEqual(digest, hashlib.sha256(bytes([0]) * len(canvas.px)).hexdigest())

    def test_png_header(self):
        canvas = preview_faces.render_mood("idle", 0)
        blob = preview_faces.png_bytes(preview_faces.W, preview_faces.H, canvas.px)
        self.assertTrue(blob.startswith(b"\x89PNG\r\n\x1a\n"))


if __name__ == "__main__":
    unittest.main()
