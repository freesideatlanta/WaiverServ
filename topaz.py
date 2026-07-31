"""Topaz signature pad reader: 8-byte packets from a serial device."""

import os

from gi.repository import GLib, GLibUnix

PACKET_LEN = 8
PEN_LIFT = 0x80
RETRY_MS = 500


class TopazPad:
    """Opens the device (retrying until it appears, reopening on error) and
    delivers batches of decoded pen events to on_events: (x, y) tuples for
    in-range points, None for pen lifts and out-of-range points."""

    def __init__(self, device, width, height, on_events):
        self.device = device
        self.width = width
        self.height = height
        self.on_events = on_events
        self.fd = None
        self.buf = bytearray()

    def start(self):
        GLib.timeout_add(RETRY_MS, self._try_open)

    def _try_open(self):
        if self.fd is None:
            try:
                self.fd = os.open(self.device, os.O_RDONLY | os.O_NONBLOCK)
            except OSError:
                return True
            self.buf.clear()
            GLibUnix.fd_add_full(
                GLib.PRIORITY_DEFAULT,
                self.fd,
                GLib.IOCondition.IN | GLib.IOCondition.HUP | GLib.IOCondition.ERR,
                self._on_data,
            )
        return True

    def _on_data(self, fd, condition):
        chunk = b""
        if condition & GLib.IOCondition.IN:
            try:
                chunk = os.read(fd, 4096)
            except BlockingIOError:
                return True
            except OSError:
                pass
        if not chunk:
            os.close(fd)
            self.fd = None
            return False
        self.buf += chunk

        events = []
        while len(self.buf) >= PACKET_LEN:
            packet = bytes(self.buf[:PACKET_LEN])
            del self.buf[:PACKET_LEN]
            if packet[1] == PEN_LIFT:
                events.append(None)
                continue
            x = (packet[2] + packet[3] * 127 - 500) // 6
            y = (packet[4] + packet[5] * 127 - 350) // 6
            events.append(
                (x, y) if 0 <= x < self.width and 0 <= y < self.height else None
            )
        if events:
            self.on_events(events)
        return True
