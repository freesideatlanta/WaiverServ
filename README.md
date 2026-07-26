# WaiverServ

This is the system the greets you when you enter the building, and prompts you to sign a liability waiver. It's boring but critical infrastructure to Freeside's long-term survival.

## Hardware requirements

- a Raspberry Pi 4 model B
- a portrait-oriented monitor
- a Topaz signature pad

## Running

```
nix run .
```

Or in a dev shell: `nix develop` then `./main.py`.

Signed waivers are written to `$WAIVER_SAVE_DIR` (required) & uploaded to a
Google Shared Drive; see [gdrive-sync.md](gdrive-sync.md) for the one-time setup.

## SD card image

```
nix build .#nixosConfigurations.waiverserv.config.system.build.sdImage
```

Building on an x86_64 machine requires binfmt_misc for aarch64
(`boot.binfmt.emulatedSystems = [ "aarch64-linux" ];` on NixOS), but only the
uncached bits are built under emulation.

You may wish to set up nixbuild.net for fast native builds.

## Flashing the SD card

The build leaves a compressed image in `result/sd-image/`. Write it to the card
(replace `/dev/sdX` with the card's device — check first with `lsblk` so you
don't wipe your whole disk!!):

```sh
zstdcat result/sd-image/*.img.zst | sudo dd of=/dev/sdX bs=4M status=progress conv=fsync
```

Then put the card in a laptop and add `wifi.txt` and your `authorized_keys` to
the `FIRMWARE` partition before first boot; see
[sd-card-config.md](sd-card-config.md).

## Updating a running kiosk

```sh
nixos-rebuild switch --flake .#waiverserv --target-host admin@192.168.1.101 --sudo
```

## Testing without a signature pad

The pad is read from `/dev/topaz`. A FIFO at that path plus `fakepad.py`
substitutes for the hardware:

```
sudo mkfifo /dev/topaz && sudo chmod 666 /dev/topaz
./main.py &
./fakepad.py
```
