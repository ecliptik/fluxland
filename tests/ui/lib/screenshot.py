"""Screenshot capture and comparison using grim + Pillow."""

import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

from PIL import Image


@dataclass
class CompareResult:
    """Result of a pixel-by-pixel image comparison."""

    match: bool
    diff_percent: float
    max_channel_diff: int
    diff_image_path: Path | None
    actual_path: Path
    reference_path: Path


class ScreenshotEngine:
    """Capture screenshots via grim and compare against reference images."""

    def __init__(
        self,
        reference_dir,
        results_dir,
        tolerance_percent=2.0,
        channel_threshold=5,
    ):
        """Initialize the screenshot engine.

        Args:
            reference_dir: Directory containing golden reference images.
            results_dir: Directory for test run screenshots and diffs.
            tolerance_percent: Max percentage of differing pixels before failure.
            channel_threshold: Per-channel difference threshold to count as different.
        """
        self.reference_dir = Path(reference_dir)
        self.results_dir = Path(results_dir)
        self.tolerance_percent = tolerance_percent
        self.channel_threshold = channel_threshold

        self._screenshots_dir = self.results_dir / "screenshots"
        self._diffs_dir = self.results_dir / "diffs"
        self._screenshots_dir.mkdir(parents=True, exist_ok=True)
        self._diffs_dir.mkdir(parents=True, exist_ok=True)

        if not shutil.which("grim"):
            raise RuntimeError("grim is not installed or not in PATH")

    def capture(self, name: str, region=None, output=None) -> Path:
        """Capture a screenshot via grim.

        Args:
            name: Base name for the screenshot file (without extension).
            region: Optional (x, y, w, h) tuple for a screen region.
            output: Optional monitor name to capture a specific output.

        Returns:
            Path to the saved PNG in results/screenshots/.
        """
        dest = self._screenshots_dir / f"{name}.png"
        cmd = ["grim"]

        if region is not None:
            x, y, w, h = region
            cmd.extend(["-g", f"{w}x{h}+{x}+{y}"])
        elif output is not None:
            cmd.extend(["-o", output])

        cmd.append(str(dest))
        subprocess.run(cmd, check=True, capture_output=True)
        return dest

    def compare(self, actual: Path, reference: Path) -> CompareResult:
        """Pixel-by-pixel comparison with tolerance.

        For each pixel, if any channel differs by more than channel_threshold
        it counts as a differing pixel. If total differing pixels exceed
        tolerance_percent of total pixels, the comparison fails.

        A diff image is generated: matching pixels at 30% brightness,
        differing pixels drawn in bright red (255, 0, 0).

        Args:
            actual: Path to the actual screenshot.
            reference: Path to the reference image.

        Returns:
            CompareResult with match status and diff details.
        """
        actual_img = Image.open(actual).convert("RGBA")
        ref_img = Image.open(reference).convert("RGBA")

        if actual_img.size != ref_img.size:
            # Size mismatch is always a failure
            diff_name = f"{actual.stem}_diff.png"
            diff_path = self._diffs_dir / diff_name
            actual_img.save(diff_path)
            return CompareResult(
                match=False,
                diff_percent=100.0,
                max_channel_diff=255,
                diff_image_path=diff_path,
                actual_path=actual,
                reference_path=reference,
            )

        width, height = actual_img.size
        total_pixels = width * height
        actual_data = actual_img.load()
        ref_data = ref_img.load()

        diff_img = Image.new("RGBA", (width, height))
        diff_data = diff_img.load()

        diff_count = 0
        max_diff = 0

        for y in range(height):
            for x in range(width):
                ap = actual_data[x, y]
                rp = ref_data[x, y]

                # Check per-channel difference
                channel_diffs = [abs(ap[c] - rp[c]) for c in range(4)]
                pixel_max = max(channel_diffs)
                if pixel_max > max_diff:
                    max_diff = pixel_max

                if pixel_max > self.channel_threshold:
                    diff_count += 1
                    diff_data[x, y] = (255, 0, 0, 255)
                else:
                    # Dim to 30% brightness
                    diff_data[x, y] = (
                        int(ap[0] * 0.3),
                        int(ap[1] * 0.3),
                        int(ap[2] * 0.3),
                        ap[3],
                    )

        diff_percent = (diff_count / total_pixels) * 100.0 if total_pixels > 0 else 0.0
        match = diff_percent <= self.tolerance_percent

        diff_path = None
        if not match:
            diff_name = f"{actual.stem}_diff.png"
            diff_path = self._diffs_dir / diff_name
            diff_img.save(diff_path)

        return CompareResult(
            match=match,
            diff_percent=diff_percent,
            max_channel_diff=max_diff,
            diff_image_path=diff_path,
            actual_path=actual,
            reference_path=reference,
        )

    def assert_matches_reference(self, name: str, region=None, masks=None):
        """Capture a screenshot and compare against its reference image.

        If no reference image exists, the actual screenshot is saved as the
        new reference (bootstrap mode) and the assertion passes.

        Args:
            name: Base name for the screenshot (without extension).
            region: Optional (x, y, w, h) for capturing a screen region.
            masks: List of (x, y, w, h) regions to exclude from comparison.

        Raises:
            AssertionError: If the screenshot does not match the reference.
        """
        actual_path = self.capture(name, region=region)
        ref_path = self.reference_dir / f"{name}.png"

        if not ref_path.exists():
            # Bootstrap: save actual as reference
            self.reference_dir.mkdir(parents=True, exist_ok=True)
            actual_img = Image.open(actual_path)
            actual_img.save(ref_path)
            return

        actual_img = Image.open(actual_path).convert("RGBA")
        ref_img = Image.open(ref_path).convert("RGBA")

        # Apply masks: zero out masked regions in both images
        if masks:
            for mx, my, mw, mh in masks:
                for img in (actual_img, ref_img):
                    data = img.load()
                    for py in range(my, min(my + mh, img.height)):
                        for px in range(mx, min(mx + mw, img.width)):
                            data[px, py] = (0, 0, 0, 0)

        # Save masked versions for comparison
        masked_actual = self._screenshots_dir / f"{name}_masked.png"
        masked_ref = self._screenshots_dir / f"{name}_masked_ref.png"
        actual_img.save(masked_actual)
        ref_img.save(masked_ref)

        result = self.compare(masked_actual, masked_ref)

        # Clean up temporary masked files
        masked_actual.unlink(missing_ok=True)
        masked_ref.unlink(missing_ok=True)

        if not result.match:
            raise AssertionError(
                f"Screenshot '{name}' does not match reference.\n"
                f"  Diff: {result.diff_percent:.2f}% pixels differ "
                f"(tolerance: {self.tolerance_percent}%)\n"
                f"  Max channel diff: {result.max_channel_diff}\n"
                f"  Diff image: {result.diff_image_path}\n"
                f"  Actual: {result.actual_path}\n"
                f"  Reference: {result.reference_path}"
            )

    def assert_region_color(self, x, y, w, h, expected_color, tolerance=10):
        """Capture a screen region and verify it is approximately a solid color.

        Args:
            x, y, w, h: Region to capture.
            expected_color: (r, g, b) or (r, g, b, a) tuple.
            tolerance: Max per-channel deviation from expected color.

        Raises:
            AssertionError: If any pixel deviates beyond tolerance.
        """
        path = self.capture("_color_check", region=(x, y, w, h))
        img = Image.open(path).convert("RGBA")
        data = img.load()

        if len(expected_color) == 3:
            expected = (*expected_color, 255)
        else:
            expected = tuple(expected_color)

        width, height = img.size
        for py in range(height):
            for px in range(width):
                pixel = data[px, py]
                for c in range(len(expected)):
                    diff = abs(pixel[c] - expected[c])
                    if diff > tolerance:
                        raise AssertionError(
                            f"Pixel ({x + px}, {y + py}) color {pixel} "
                            f"deviates from expected {expected} "
                            f"by {diff} on channel {c} (tolerance: {tolerance})"
                        )

    def assert_region_not_empty(self, x, y, w, h):
        """Capture a screen region and verify it is not blank.

        A region is considered blank if all pixels are the same color.

        Args:
            x, y, w, h: Region to capture.

        Raises:
            AssertionError: If the region is blank (all same color).
        """
        path = self.capture("_empty_check", region=(x, y, w, h))
        img = Image.open(path).convert("RGBA")
        data = img.load()

        width, height = img.size
        if width == 0 or height == 0:
            raise AssertionError(f"Region ({x}, {y}, {w}, {h}) has zero size")

        first_pixel = data[0, 0]
        for py in range(height):
            for px in range(width):
                if data[px, py] != first_pixel:
                    return

        raise AssertionError(
            f"Region ({x}, {y}, {w}, {h}) is blank — "
            f"all pixels are {first_pixel}"
        )
