"""E2E tests, run under xvfb-run. GTK4 has no pointer-event injection, so a
"click" here picks the widget at real overlay coordinates (verifying layout,
visibility and hit-targets) and fires its click gesture. Pad input goes
through a real FIFO into the TopazPad decoder."""

import json
import math
import os
import tempfile
import time

os.environ["WAIVER_SAVE_DIR"] = tempfile.mkdtemp(prefix="waivers.")
os.environ["DBUS_SYSTEM_BUS_ADDRESS"] = os.environ["DBUS_SESSION_BUS_ADDRESS"]

import cairo
import gi

gi.require_version("Gtk", "4.0")
from gi.repository import Gio, GLib, Gtk
import pytest

import main

PEN_LIFT = bytes([0, 0x80, 0, 0, 0, 0, 0, 0])

SYSTEMD_XML = """
<node>
  <interface name='org.freedesktop.systemd1.Manager'>
    <method name='StartUnit'>
      <arg type='s' direction='in'/>
      <arg type='s' direction='in'/>
      <arg type='o' direction='out'/>
    </method>
  </interface>
</node>
"""


def pump():
    ctx = GLib.MainContext.default()
    while ctx.pending():
        ctx.iteration(False)


def wait_until(cond, timeout=5.0):
    ctx = GLib.MainContext.default()
    deadline = time.monotonic() + timeout
    while not cond():
        assert time.monotonic() < deadline, "timed out"
        ctx.iteration(True)


def controllers(widget):
    model = widget.observe_controllers()
    return [model.get_item(i) for i in range(model.get_n_items())]


def fire(widget, x, y):
    while widget is not None:
        if isinstance(widget, Gtk.Button):
            widget.emit("clicked")
            pump()
            return
        for ctl in controllers(widget):
            if isinstance(ctl, Gtk.GestureClick):
                ctl.emit("pressed", 1, float(x), float(y))
                pump()
                return
        widget = widget.get_parent()
    raise AssertionError(f"nothing clickable at ({x}, {y})")


def click_at(app, x, y):
    pump()
    picked = app.overlay.pick(x, y, Gtk.PickFlags.DEFAULT)
    assert picked is not None
    fire(picked, x, y)


def click(app, widget):
    wait_until(lambda: widget.compute_bounds(app.overlay)[1].get_width() > 0)
    b = widget.compute_bounds(app.overlay)[1]
    x, y = b.get_x() + b.get_width() / 2, b.get_y() + b.get_height() / 2
    picked = app.overlay.pick(x, y, Gtk.PickFlags.DEFAULT)
    assert picked is widget or picked.is_ancestor(widget), (
        f"expected {widget} at ({x}, {y}), got {picked}"
    )
    fire(picked, x, y)


def packet(x, y):
    xr, yr = 6 * x + 500, 6 * y + 350
    return bytes([0, 0, xr % 127, xr // 127, yr % 127, yr // 127, 0, 0])


def ink_points(field):
    return sum(
        field.data[o : o + 3] == b"\x00\x00\x00" for o in range(0, len(field.data), 4)
    )


def sign(app, pad, field):
    click(app, field.sign_prompt)
    assert field.active and field.picture.get_visible()
    strokes = b"".join(packet(x, 40 + int(20 * math.sin(x / 9))) for x in range(250))
    os.write(pad, strokes + PEN_LIFT)
    wait_until(lambda: ink_points(field) >= main.MIN_SIG_POINTS)


def enter_name(app, field, name):
    click(app, field.name_label)
    assert field.entry.get_visible()
    field.entry.set_text(name)
    click_at(app, app.overlay.get_width() / 2, 5)
    assert field.name == name and field.name_label.get_text() == name


def saved():
    return sorted(main.SAVE_DIR.glob("*/*.png"))


@pytest.fixture(scope="session")
def app():
    fifo = os.path.join(tempfile.mkdtemp(prefix="pad."), "topaz")
    os.mkfifo(fifo)
    main.TOUCHPAD_DEV = fifo
    a = main.WaiverApp()
    a.register()
    a.activate()
    wait_until(lambda: a.main_sig.sign_prompt.get_width() > 0)
    wait_until(lambda: a.pad.fd is not None)
    return a


@pytest.fixture(scope="session")
def systemd():
    """Stands in for PID 1 on the bus main.start_sync() talks to."""
    bus = Gio.bus_get_sync(Gio.BusType.SYSTEM, None)
    calls = []

    def on_call(conn, sender, path, iface, method, params, invocation):
        calls.append(tuple(params))
        invocation.return_value(
            GLib.Variant("(o)", ("/org/freedesktop/systemd1/job/1",))
        )

    node = Gio.DBusNodeInfo.new_for_xml(SYSTEMD_XML)
    bus.register_object("/org/freedesktop/systemd1", node.interfaces[0], on_call)
    owned = []
    Gio.bus_own_name_on_connection(
        bus,
        "org.freedesktop.systemd1",
        Gio.BusNameOwnerFlags.NONE,
        lambda *a: owned.append(True),
    )
    wait_until(lambda: owned)
    return calls


@pytest.fixture(scope="session")
def pad(app):
    fd = os.open(main.TOUCHPAD_DEV, os.O_WRONLY)
    yield fd
    os.close(fd)


@pytest.fixture(autouse=True)
def fresh(app):
    yield
    app.reset_screen()
    app.hide_status()
    pump()


def test_empty_submit_shows_error(app):
    before = saved()
    click(app, app.buttons[0][0])
    assert app.status_label.get_visible()
    assert "Red Fields" in app.status_label.get_text()
    assert saved() == before


def test_signed_waiver_saved(app, pad, systemd):
    sign(app, pad, app.main_sig)
    enter_name(app, app.main_sig, "Jane Doe")
    before = saved()
    click(app, app.buttons[0][0])
    new = [f for f in saved() if f not in before]
    assert len(new) == 1
    png = cairo.ImageSurface.create_from_png(str(new[0]))
    assert (png.get_width(), png.get_height()) == (main.IMG_W, main.IMG_H)
    meta = json.loads(new[0].with_suffix(".json").read_text())
    assert meta["name"] == "Jane Doe" and meta["minor"] is False
    assert new[0].parent.name == time.strftime("%Y")
    assert "Thank You" in app.status_label.get_text()
    wait_until(lambda: systemd)
    assert systemd[-1] == (main.SYNC_UNIT, "replace")


def test_reclicking_name_prefills_entry(app):
    enter_name(app, app.main_sig, "Jane Doe")
    click(app, app.main_sig.name_label)
    assert app.main_sig.entry.get_text() == "Jane Doe"
