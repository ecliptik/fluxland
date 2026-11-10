"""X11 windows with a Shape, and how override-redirect windows stack.

Wine's layered windows (Proton's focus overlays, for one) are 32-bit
override-redirect windows whose X Shape cuts the inside out: the ring is the
window, the rest of the rectangle is not there.  Wayland has no shapes;
XWayland leaves the part outside it black, so a compositor has to clip to the
shape itself.  fluxland-test-xshape makes such a window.

An override-redirect window is an ordinary one in X's stacking order: on top
when it is mapped or when its client raises it, below a window the window
manager raises afterwards.
"""

import io
import os
import shutil
import signal
import subprocess
import time

import pytest
from PIL import Image

from lib.compositor import CompositorManager
from lib.input_driver import _find_test_pointer
from lib.ipc_client import FluxlandIPC

SETTLE = 0.6
RING = (224, 255, 255)

pytestmark = pytest.mark.skipif(
    not (shutil.which("swaybg") and shutil.which("grim")
         and shutil.which("foot") and shutil.which("Xwayland")),
    reason="needs swaybg, grim, foot and Xwayland")


def xshape_binary():
    pointer = _find_test_pointer()
    if pointer:
        path = os.path.join(os.path.dirname(pointer), "fluxland-test-xshape")
        if os.access(path, os.X_OK):
            return path
    return None


@pytest.fixture(scope="module")
def comp():
    if not xshape_binary():
        pytest.skip("fluxland-test-xshape is not built")
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True, outputs=1,
              init_extra=["session.screen0.toolbar.visible: false"])
    bg = subprocess.Popen(["swaybg", "-c", "#ff0000"], env=mgr.client_env,
                          stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL)
    time.sleep(1.0)
    yield mgr
    bg.terminate()
    bg.wait(timeout=5)
    mgr.stop()


@pytest.fixture
def ipc(comp):
    client = FluxlandIPC(socket_path=comp.socket_path)
    yield client
    try:
        client.command("CloseAllWindows")
        time.sleep(0.3)
    except Exception:
        pass
    client.close()


@pytest.fixture(scope="module")
def x_display(comp, tmp_path_factory):
    """The compositor's DISPLAY, as its children see it."""
    path = tmp_path_factory.mktemp("xdisplay") / "display"
    client = FluxlandIPC(socket_path=comp.socket_path)
    client.command(f"Exec sh -c 'echo $DISPLAY > {path}'")
    client.close()
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if path.exists() and path.read_text().strip():
            return path.read_text().strip()
        time.sleep(0.1)
    raise TimeoutError("no DISPLAY")


@pytest.fixture
def xclients(comp, x_display):
    """Start X test windows; they are ended afterwards."""
    started = []

    def start(*args):
        env = {**comp.client_env, "DISPLAY": x_display}
        proc = subprocess.Popen([xshape_binary(), *map(str, args)], env=env,
                                stdout=subprocess.PIPE, text=True)
        assert proc.stdout.readline().startswith("ready")
        started.append(proc)
        time.sleep(SETTLE)
        return proc

    yield start
    for proc in started:
        proc.send_signal(signal.SIGTERM)
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()


def screen(comp):
    png = subprocess.run(["grim", "-t", "png", "-"], check=True,
                         capture_output=True, env=comp.client_env).stdout
    return Image.open(io.BytesIO(png)).convert("RGB")


def close(a, b, tol=6):
    return all(abs(p - q) <= tol for p, q in zip(a, b))


def test_the_inside_of_a_ring_is_not_black(comp, xclients):
    xclients("--x", 300, "--y", 200, "--ring")
    img = screen(comp)
    # on the ring: the window's own pixels
    assert close(img.getpixel((302, 202)), RING)
    # inside it, where the shape leaves the window out: the wallpaper
    assert close(img.getpixel((340, 216)), (255, 0, 0))


def test_wines_overlay_shape(comp, xclients):
    # Wine's four rectangles for an 85x33 window reach beyond the window
    xclients("--x", 300, "--y", 200, "--wine")
    img = screen(comp)
    assert close(img.getpixel((302, 202)), RING)
    assert close(img.getpixel((340, 216)), (255, 0, 0))
    # nothing outside the window
    assert close(img.getpixel((300 + 90, 200 + 40)), (255, 0, 0))


def test_an_unshaped_window_is_whole(comp, xclients):
    xclients("--x", 300, "--y", 200, "--w", 85, "--h", 33)
    img = screen(comp)
    # the inside is transparent (alpha 0), not left out: the wallpaper too,
    # and the ring is there
    assert close(img.getpixel((302, 202)), RING)
    assert close(img.getpixel((340, 216)), (255, 0, 0))


GREEN = (0, 255, 0)


def test_what_the_shape_leaves_out_is_not_drawn(comp, xclients):
    # an opaque inside: only the shape keeps it from showing
    xclients("--x", 300, "--y", 200, "--ring", "--fill")
    img = screen(comp)
    assert close(img.getpixel((302, 202)), RING)
    assert close(img.getpixel((340, 216)), (255, 0, 0))


def test_the_shape_changing_is_followed(comp, xclients):
    proc = xclients("--x", 300, "--y", 200, "--ring", "--fill")
    assert close(screen(comp).getpixel((340, 216)), (255, 0, 0))

    # shaped off (X sends ShapeNotify): the whole window, the inside too
    proc.send_signal(signal.SIGUSR2)
    time.sleep(SETTLE)
    img = screen(comp)
    assert close(img.getpixel((340, 216)), GREEN)
    assert close(img.getpixel((302, 202)), RING)

    # and on again
    proc.send_signal(signal.SIGUSR2)
    time.sleep(SETTLE)
    assert close(screen(comp).getpixel((340, 216)), (255, 0, 0))


def test_a_managed_window_is_clipped_too(comp, xclients, ipc):
    xclients("--managed", "--title", "Shaped", "--x", 300, "--y", 200,
             "--w", 120, "--h", 80, "--ring", "--fill")
    win = ipc.get_window_by_title("Shaped")
    assert win, "the shaped window is managed"
    f = win["frame"]
    img = screen(comp)
    # the content is inside the frame (titlebar, borders): its middle is
    # in the shape's hole, which shows what is behind
    cx = win["x"] + win["width"] // 2
    cy = win["y"] + win["height"] // 2
    assert close(img.getpixel((cx, cy)), (255, 0, 0)), (f, img.getpixel((cx, cy)))


# --- Stacking: an override-redirect window is on top when it appears, and a
# window raised after that covers it (a terminal clicked over Proton's focus
# overlay); one of the same application, and a popup, stay over ---

BLUE = (0, 0, 255)
OVERLAY = (320, 230)	# where the stacking tests put the ring window
RING_AT = (322, 232)	# a pixel on its ring


def open_foot(ipc, app_id, color, frame):
    """A foot window of a flat color, frame (x, y, w, h)."""
    ipc.command(f"Exec foot --app-id {app_id} "
                f"-o colors-dark.background={color} -o colors-dark.alpha=1.0")
    deadline = time.monotonic() + 5
    win = None
    while time.monotonic() < deadline:
        win = ipc.get_window_by_app_id(app_id)
        if win and win["width"] > 0:
            break
        time.sleep(0.1)
    else:
        raise TimeoutError(app_id)
    place(ipc, win, frame)
    return win


def place(ipc, win, frame):
    x, y, w, h = frame
    ipc.command(f"MoveTo {x} {y}", window=win["id"])
    ipc.command(f"ResizeTo {w} {h}", window=win["id"])
    time.sleep(SETTLE)


def top_at(comp, point):
    return screen(comp).getpixel(point)


def ring_visible(comp):
    return close(top_at(comp, RING_AT), RING)


def test_a_window_mapped_after_covers_an_override_redirect_one(comp, ipc,
                                                               xclients):
    xclients("--x", OVERLAY[0], "--y", OVERLAY[1], "--ring", "--fill")
    assert ring_visible(comp)
    open_foot(ipc, "Later", "0000ff", (200, 150, 400, 300))
    assert close(top_at(comp, RING_AT), BLUE)


def test_an_override_redirect_window_appears_on_top(comp, ipc, xclients):
    open_foot(ipc, "Earlier", "0000ff", (200, 150, 400, 300))
    xclients("--x", OVERLAY[0], "--y", OVERLAY[1], "--ring", "--fill")
    assert ring_visible(comp)


def test_a_window_raised_after_covers_it(comp, ipc, xclients):
    under = open_foot(ipc, "Under", "0000ff", (200, 150, 400, 300))
    # another window on top of it, away from the ring window, so that the
    # first is behind it
    other = open_foot(ipc, "Other", "ffff00", (700, 400, 300, 200))
    xclients("--x", OVERLAY[0], "--y", OVERLAY[1], "--ring", "--fill")
    assert ring_visible(comp)

    ipc.command("Raise", window=other["id"])
    time.sleep(SETTLE)
    assert ring_visible(comp), "a window it does not touch is no matter"
    ipc.command("Raise", window=under["id"])
    time.sleep(SETTLE)
    assert close(top_at(comp, RING_AT), BLUE)

    # raising another does not bring it back over the first
    ipc.command("Raise", window=other["id"])
    time.sleep(SETTLE)
    assert close(top_at(comp, RING_AT), BLUE)


def test_a_window_of_the_same_application_stays_under_it(comp, ipc, xclients):
    # M and the ring window share WM_CLASS: the overlay is for M
    xclients("--managed", "--class", "grp", "--title", "Grp", "--fill",
             "--x", 100, "--y", 100, "--w", 300, "--h", 200)
    member = ipc.get_window_by_title("Grp")
    place(ipc, member, (300, 200, 300, 250))
    other = open_foot(ipc, "Covers", "0000ff", (310, 180, 300, 200))
    xclients("--x", OVERLAY[0], "--y", OVERLAY[1], "--ring", "--fill",
             "--class", "grp")
    assert ring_visible(comp)

    ipc.command("Raise", window=other["id"])
    time.sleep(SETTLE)
    assert close(top_at(comp, RING_AT), BLUE), "a stranger covers it"

    ipc.command("Raise", window=member["id"])
    time.sleep(SETTLE)
    assert ring_visible(comp), "its own application's window does not"


def test_a_popup_stays_over_a_raised_window(comp, ipc, xclients):
    under = open_foot(ipc, "UnderPopup", "0000ff", (200, 150, 400, 300))
    xclients("--x", OVERLAY[0], "--y", OVERLAY[1], "--ring", "--fill",
             "--popup")
    assert ring_visible(comp)
    ipc.command("Raise", window=under["id"])
    time.sleep(SETTLE)
    assert ring_visible(comp)
