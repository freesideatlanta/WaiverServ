#!/usr/bin/env python3
import json
import logging
import os
import time
from pathlib import Path

import cairo
import gi
gi.require_version("Gtk", "4.0")
gi.require_version("Gdk", "4.0")
from gi.repository import Gdk, Gio, GLib, Gtk

from topaz import TopazPad

SIG_W, SIG_H = 300, 85
SIG_STRIDE = SIG_W * 4
MIN_SIG_POINTS = 30
HIGHLIGHT = b"\xf4\xf4\x70\x80"
INK = b"\x00\x00\x00\xff"

IMG_W, IMG_H = 1697, 2400
SIG_ROWS = (1795, 2011)
SIG_LINE_X, SIG_LINE_W = 306, 593
NAME_LINE_X, NAME_LINE_W = 365, 541
DATE_X = 1005
NAME_DY = 72
ROW_H = 47
FONT_PX = 33
STATUS_SECONDS = 3
RESET_SECONDS = 4
SIG_DISP_H = SIG_LINE_W * SIG_H // SIG_W
BTN_STRIP = 140

SIGN_PROMPT = "CLICK HERE TO SIGN"
NAME_PROMPT = "CLICK HERE TO ENTER NAME"
AGE_PROMPT = "CLICK HERE IF UNDER 18"
UNDER_18 = "UNDER 18"

TOUCHPAD_DEV = "/dev/topaz"
SYNC_UNIT = "waiverserv-sync.service"
SAVE_DIR = Path(os.environ["WAIVER_SAVE_DIR"])
WAIVER_IMG = Path(__file__).resolve().parent / "FS_Waiver_Apr_2019.png"

STYLES = ("unfilled", "confirm", "error", "info")

log = logging.getLogger(__name__)

CSS = """
.field, .status { font-size: %dpx; }
.field    { color: #000000; }
.unfilled { background-color: #f79891; }
.confirm  { background-color: #83f939; }
entry.field { background-image: none; background-color: #ffffff; }
.status { padding: 40px; border-radius: 20px; color: #ffffff; }
.status.error { background-color: #c62828; }
.status.info  { background-color: #2e7d32; }
"""


def set_style(widget, style=None):
    for cls in STYLES:
        widget.remove_css_class(cls)
    if style:
        widget.add_css_class(style)


def sync_finished(bus, result, *_):
    try:
        bus.call_finish(result)
    except GLib.Error:
        log.exception("failed to start %s", SYNC_UNIT)


def start_sync():
    try:
        bus = Gio.bus_get_sync(Gio.BusType.SYSTEM, None)
        bus.call("org.freedesktop.systemd1", "/org/freedesktop/systemd1",
                 "org.freedesktop.systemd1.Manager", "StartUnit",
                 GLib.Variant("(ss)", (SYNC_UNIT, "replace")),
                 GLib.VariantType("(o)"), Gio.DBusCallFlags.NONE, -1, None,
                 sync_finished)
    except GLib.Error:
        log.exception("no system bus, skipping %s", SYNC_UNIT)


def write_durably(path, write):
    tmp = path.with_name(path.name + ".tmp")
    with open(tmp, "wb") as f:
        write(f)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)
    dir_fd = os.open(path.parent, os.O_RDONLY)
    try:
        os.fsync(dir_fd)
    finally:
        os.close(dir_fd)


def positioned(widget, top, left):
    widget.set_halign(Gtk.Align.START)
    widget.set_valign(Gtk.Align.START)
    widget.set_margin_top(top)
    widget.set_margin_start(left)
    return widget


class Layout:
    def __init__(self, scale, off_x, off_y, screen_w, screen_h):
        self.scale = scale
        self.off_x = off_x
        self.off_y = off_y
        self.screen_w = screen_w
        self.screen_h = screen_h

    def place(self, widget, top, left):
        return positioned(widget, self.off_y + int(top * self.scale),
                          self.off_x + int(left * self.scale))

    def box(self, widget, top, left, w, h):
        t = self.off_y + int(top * self.scale)
        l = self.off_x + int(left * self.scale)
        widget.set_margin_top(t)
        widget.set_margin_start(l)
        widget.set_margin_end(self.screen_w - l - int(w * self.scale))
        widget.set_margin_bottom(self.screen_h - t - int(h * self.scale))
        return widget

    def size(self, widget, w, h):
        widget.set_size_request(int(w * self.scale), int(h * self.scale))
        return widget


def field_label(text=""):
    label = Gtk.Label(label=text)
    label.set_xalign(0)
    label.add_css_class("field")
    return label


def prompt_label(text):
    label = field_label(text)
    set_style(label, "unfilled")
    return label


def add_click(widget, handler):
    gesture = Gtk.GestureClick()

    def pressed(g, _n, _x, _y):
        g.set_state(Gtk.EventSequenceState.CLAIMED)
        handler()

    gesture.connect("pressed", pressed)
    widget.add_controller(gesture)


def draw_text(cr, text, x, line_y):
    ascent, descent = cr.font_extents()[:2]
    cr.move_to(x, line_y - ROW_H + (ROW_H - ascent - descent) / 2 + ascent)
    cr.show_text(text)


class SignatureField:
    def __init__(self, overlay, sig_y, finalize_all, on_data=None):
        self.sig_y = sig_y
        self.finalize_all = finalize_all
        self.on_data = on_data
        self.data = bytearray(SIG_STRIDE * SIG_H)
        self.name = ""
        self.active = False
        self.valid = False

        self.picture = Gtk.Picture()
        self.picture.set_content_fit(Gtk.ContentFit.FILL)
        add_click(self.picture, self._on_sign_click)

        self.sign_prompt = prompt_label(SIGN_PROMPT)
        add_click(self.sign_prompt, self._on_sign_click)

        self.name_label = prompt_label(NAME_PROMPT)
        add_click(self.name_label, self._on_name_click)

        self.entry = Gtk.Entry()
        self.entry.add_css_class("field")
        self.entry.connect("activate", lambda _e: self.finalize_all())

        for w in (self.sign_prompt, self.picture, self.name_label, self.entry):
            overlay.add_overlay(w)
        self.picture.set_visible(False)
        self.entry.set_visible(False)

    def relayout(self, layout):
        name_y = self.sig_y + NAME_DY
        layout.box(self.picture, self.sig_y - SIG_DISP_H, SIG_LINE_X,
                   SIG_LINE_W, SIG_DISP_H)
        layout.place(layout.size(self.sign_prompt, SIG_LINE_W, ROW_H),
                     self.sig_y - ROW_H, SIG_LINE_X)
        layout.place(layout.size(self.name_label, NAME_LINE_W, ROW_H),
                     name_y - ROW_H, NAME_LINE_X)
        layout.place(layout.size(self.entry, NAME_LINE_W, ROW_H),
                     name_y - ROW_H, NAME_LINE_X)

    def refresh(self):
        texture = Gdk.MemoryTexture.new(
            SIG_W, SIG_H, Gdk.MemoryFormat.R8G8B8A8,
            GLib.Bytes.new(bytes(self.data)), SIG_STRIDE)
        self.picture.set_paintable(texture)

    def render_name(self):
        if self.name:
            self.name_label.set_text(self.name)
            set_style(self.name_label)
        else:
            self.name_label.set_text(NAME_PROMPT)
            set_style(self.name_label, "unfilled")

    def _on_sign_click(self):
        self.finalize_all()
        self.sign_prompt.set_visible(False)
        self.picture.set_visible(True)
        self.data[:] = HIGHLIGHT * (SIG_W * SIG_H)
        self.refresh()
        self.active = True
        self.valid = False

    def _on_name_click(self):
        self.finalize_all()
        self.name_label.set_visible(False)
        self.entry.set_text(self.name)
        self.entry.set_visible(True)
        self.entry.grab_focus()

    def draw_line(self, a, b):
        (x0, y0), (x1, y1) = a, b
        steps = max(abs(x1 - x0), abs(y1 - y0), 1)
        for i in range(steps + 1):
            x = x0 + (x1 - x0) * i // steps
            y = y0 + (y1 - y0) * i // steps
            o = y * SIG_STRIDE + x * 4
            self.data[o:o + 4] = INK

    def _flatten(self):
        points = 0
        for o in range(0, len(self.data), 4):
            if self.data[o:o + 3] == b"\x00\x00\x00":
                self.data[o + 3] = 0xFF
                points += 1
            else:
                self.data[o:o + 4] = b"\x00\x00\x00\x00"
        return points

    def finalize(self):
        entered = False

        if self.active:
            self.active = False
            points = self._flatten()
            self.valid = points >= MIN_SIG_POINTS
            log.info("signature at y=%d: %d points (min %d), valid=%s",
                     self.sig_y, points, MIN_SIG_POINTS, self.valid)
            self.refresh()
            if self.valid:
                entered = True
            else:
                self.sign_prompt.set_visible(True)
                self.picture.set_visible(False)

        if self.entry.get_visible():
            text = self.entry.get_text()
            if text:
                self.name = text
                entered = True
            self.render_name()
            self.entry.set_visible(False)
            self.name_label.set_visible(True)

        if entered and self.on_data:
            self.on_data()

    @property
    def complete(self):
        return self.valid and bool(self.name)

    def reset(self):
        self.active = False
        self.valid = False
        self.name = ""
        self.sign_prompt.set_visible(True)
        self.picture.set_visible(False)
        self.render_name()
        self.name_label.set_visible(True)
        self.entry.set_visible(False)

    def hide_all(self):
        for w in (self.sign_prompt, self.picture, self.name_label, self.entry):
            w.set_visible(False)


class WaiverApp(Gtk.Application):
    def __init__(self):
        super().__init__(application_id="com.flaviutamas.WaiverServ")
        self.window = None
        self.date = ""
        self.last = None
        self.is_minor = False
        self.submit_confirmed = False
        self.status_source = 0
        self.reset_source = 0

    def do_activate(self):
        if self.window:
            self.window.present()
            return

        self.provider = Gtk.CssProvider()
        Gtk.StyleContext.add_provider_for_display(
            Gdk.Display.get_default(), self.provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

        self.window = Gtk.ApplicationWindow(application=self)
        geo = Gdk.Display.get_default().get_monitors().get_item(0).get_geometry()
        self.window.set_default_size(geo.width, geo.height)
        self.window.fullscreen()

        self.waiver_surface = cairo.ImageSurface.create_from_png(str(WAIVER_IMG))
        self.waiver = Gtk.Picture.new_for_filename(str(WAIVER_IMG))
        self.waiver.set_content_fit(Gtk.ContentFit.FILL)

        self.overlay = Gtk.Overlay()
        self.overlay.set_child(self.waiver)

        self.main_sig = SignatureField(self.overlay, SIG_ROWS[0], self.finalize_all)
        self.parent_sig = SignatureField(self.overlay, SIG_ROWS[1], self.finalize_all,
                                         on_data=self.on_parent_entered)

        self.minor_label = prompt_label(AGE_PROMPT)
        add_click(self.minor_label, self.on_minor_click)
        self.overlay.add_overlay(self.minor_label)

        self.date_labels = [field_label() for _ in SIG_ROWS]
        for label in self.date_labels:
            self.overlay.add_overlay(label)
        self.update_dates()

        self.buttons = []
        for text, quarter, handler in (("SUBMIT", 1, self.on_submit),
                                       ("CANCEL", 3, self.on_cancel)):
            button = Gtk.Button(label=text)
            button.set_size_request(200, 100)
            button.connect("clicked", handler)
            self.overlay.add_overlay(button)
            self.buttons.append((button, quarter))

        self.status_label = Gtk.Label()
        self.status_label.add_css_class("status")
        self.status_label.set_halign(Gtk.Align.CENTER)
        self.status_label.set_valign(Gtk.Align.CENTER)
        self.status_label.set_visible(False)
        self.overlay.add_overlay(self.status_label)

        catcher = Gtk.GestureClick()
        catcher.connect("pressed", self.on_global_click)
        self.overlay.add_controller(catcher)

        keys = Gtk.EventControllerKey()
        keys.connect("key-pressed", self.on_key_pressed)
        self.window.add_controller(keys)

        self.window.set_child(self.overlay)
        self.parent_sig.hide_all()
        self.window.present()

        surface = self.window.get_surface()
        surface.connect("notify::width", self.queue_relayout)
        surface.connect("notify::height", self.queue_relayout)
        self.queue_relayout()

        log.info("started: screen %dx%d, pad %s, save dir %s",
                 geo.width, geo.height, TOUCHPAD_DEV, SAVE_DIR)
        self.pad = TopazPad(TOUCHPAD_DEV, SIG_W, SIG_H, self.on_pad_events)
        self.pad.start()
        GLib.timeout_add_seconds(20, self.update_dates)

    def queue_relayout(self, *_args):
        GLib.idle_add(self.relayout)

    def relayout(self):
        w, h = self.overlay.get_width(), self.overlay.get_height()
        scale = min(w / IMG_W, (h - BTN_STRIP) / IMG_H)
        if scale <= 0:
            return
        disp_w, disp_h = int(IMG_W * scale), int(IMG_H * scale)
        layout = Layout(scale, (w - disp_w) // 2, (h - BTN_STRIP - disp_h) // 2, w, h)

        self.provider.load_from_string(CSS % int(FONT_PX * scale))
        layout.box(self.waiver, 0, 0, IMG_W, IMG_H)
        self.main_sig.relayout(layout)
        self.parent_sig.relayout(layout)
        layout.place(layout.size(self.minor_label, 480, ROW_H), 2083 - ROW_H, 1000)
        for label, y in zip(self.date_labels, SIG_ROWS):
            layout.place(label, y - ROW_H, DATE_X)
        btn_top = layout.off_y + disp_h + 20
        for button, quarter in self.buttons:
            positioned(button, btn_top, layout.off_x + quarter * disp_w // 4 - 100)

    def finalize_all(self):
        self.last = None
        self.main_sig.finalize()
        self.parent_sig.finalize()

    def on_parent_entered(self):
        self.is_minor = True
        self.minor_label.set_text(UNDER_18)
        set_style(self.minor_label, "confirm")

    def on_minor_click(self):
        self.finalize_all()
        self.is_minor = not self.is_minor
        if self.is_minor:
            self.minor_label.set_text(UNDER_18)
            set_style(self.minor_label, "confirm")
            self.parent_sig.reset()
        else:
            self.minor_label.set_text(AGE_PROMPT)
            set_style(self.minor_label, "unfilled")
            self.parent_sig.hide_all()

    def on_global_click(self, _gesture, _n, x, y):
        target = self.overlay.pick(x, y, Gtk.PickFlags.DEFAULT)
        for entry in (self.main_sig.entry, self.parent_sig.entry):
            if target and (target == entry or target.is_ancestor(entry)):
                return
        self.finalize_all()

    def on_key_pressed(self, _controller, keyval, _keycode, _state):
        if keyval in (Gdk.KEY_Return, Gdk.KEY_Tab):
            self.finalize_all()
            return True
        return False

    def on_submit(self, _button):
        self.finalize_all()
        if self.submit_confirmed:
            return
        ok = self.main_sig.complete and (not self.is_minor or self.parent_sig.complete)
        if not ok:
            log.info("submit rejected: main sig=%s name=%r, minor=%s, "
                     "parent sig=%s name=%r",
                     self.main_sig.valid, self.main_sig.name, self.is_minor,
                     self.parent_sig.valid, self.parent_sig.name)
            self.show_status("Error: Please Fill Out Red Fields.", "error")
        elif self.save_waiver():
            self.submit_confirmed = True
            self.show_status("Thank You For Your Submission.", "info")
            self.reset_source = GLib.timeout_add_seconds(RESET_SECONDS, self.auto_reset)
        else:
            self.show_status("Error: Could Not Save Waiver!", "error")

    def on_cancel(self, _button):
        self.reset_screen()

    def show_status(self, text, style):
        if self.status_source:
            GLib.source_remove(self.status_source)
        self.status_label.set_text(text)
        set_style(self.status_label, style)
        self.status_label.set_visible(True)
        self.status_source = GLib.timeout_add_seconds(STATUS_SECONDS, self.hide_status)

    def hide_status(self):
        self.status_source = 0
        self.status_label.set_visible(False)
        return False

    def auto_reset(self):
        self.reset_source = 0
        self.reset_screen()
        return False

    def reset_screen(self):
        if self.reset_source:
            GLib.source_remove(self.reset_source)
            self.reset_source = 0
        self.submit_confirmed = False
        self.main_sig.reset()
        self.parent_sig.reset()
        self.parent_sig.hide_all()
        self.is_minor = False
        self.minor_label.set_text(AGE_PROMPT)
        set_style(self.minor_label, "unfilled")

    def save_waiver(self):
        surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, IMG_W, IMG_H)
        cr = cairo.Context(surface)
        cr.set_source_surface(self.waiver_surface, 0, 0)
        cr.paint()
        cr.select_font_face("Sans", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
        cr.set_font_size(FONT_PX)

        fields = (self.main_sig, self.parent_sig) if self.is_minor else (self.main_sig,)
        for field in fields:
            sig = cairo.ImageSurface.create_for_data(
                field.data, cairo.FORMAT_ARGB32, SIG_W, SIG_H, SIG_STRIDE)
            cr.save()
            cr.translate(SIG_LINE_X, field.sig_y - SIG_DISP_H)
            cr.scale(SIG_LINE_W / SIG_W, SIG_DISP_H / SIG_H)
            cr.set_source_surface(sig, 0, 0)
            cr.paint()
            cr.restore()
            cr.set_source_rgb(0, 0, 0)
            draw_text(cr, field.name, NAME_LINE_X, field.sig_y + NAME_DY)
            draw_text(cr, self.date, DATE_X, field.sig_y)

        out_dir = SAVE_DIR / time.strftime("%Y")
        path = out_dir / f"FSwaiver_{time.strftime('%Y%m%d_%H%M%S')}.png"
        meta = {
            "signed_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
            "date": self.date,
            "minor": self.is_minor,
            "name": self.main_sig.name,
            "parent_name": self.parent_sig.name if self.is_minor else None,
            "image": path.name,
        }
        try:
            out_dir.mkdir(parents=True, exist_ok=True)
            # rclone syncs SAVE_DIR live; flake.nix excludes the .tmp names from the copy.
            write_durably(path, surface.write_to_png)
            write_durably(path.with_suffix(".json"),
                          lambda f: f.write(json.dumps(meta, indent=2).encode()))
        except (OSError, cairo.Error):
            log.exception("failed to save %s", path)
            return False
        log.info("saved %s", path)
        start_sync()
        return True

    def update_dates(self):
        self.date = time.strftime("%B %d, %Y")
        for label in self.date_labels:
            label.set_text(self.date)
        return True

    def on_pad_events(self, events):
        field = next((f for f in (self.main_sig, self.parent_sig) if f.active), None)
        drew = False
        for point in events:
            if point is None:
                self.last = None
            elif field:
                if self.last:
                    field.draw_line(self.last, point)
                self.last = point
                drew = True
        if drew:
            field.refresh()


if __name__ == "__main__":
    logging.basicConfig(
        level=logging.INFO,
        format="%(levelname)s %(name)s: %(message)s")
    raise SystemExit(WaiverApp().run(None))
