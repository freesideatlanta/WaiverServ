## Screenshotting headless

```
nix build
nix shell nixpkgs#xvfb-run nixpkgs#imagemagick nixpkgs#dbus -c \
  xvfb-run -a -s "-screen 0 1200x1920x24" dbus-run-session -- bash -c '
    env -u WAYLAND_DISPLAY GDK_BACKEND=x11 GSK_RENDERER=cairo ./result/bin/waiverserv &
    sleep 4
    import -window root shot.png
    pkill -f share/waiverserv/main.py'
```

There is no window manager on the Xvfb display, so `fullscreen()` is
best-effort — fine for checking layout: the window's default size is the
monitor size, and the layout follows the window.
