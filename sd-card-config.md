# Configuring a kiosk from its SD card

Wi-Fi and Google Drive credentials are read at every boot from the SD card's
first partition (labelled `FIRMWARE`), so they can be changed without
rebuilding the image. Shut the kiosk down, pull the card, put it in a laptop —
the partition mounts automatically on Windows, macOS and Linux — edit the
files, put the card back.

The `waiverserv-provision` service copies them into place on boot. Files that
aren't present are left alone, so an existing configuration survives.

## Wi-Fi

Create `wifi.txt` on the partition: network name on the first line, password on
the second.

```
Freeside
hunter2hunter2
```

Leave the second line empty for an open network. To check it worked, SSH in
(see below) and run:

```sh
systemctl status waiverserv-provision
sudo wpa_cli status
```

## SSH access

The kiosk has no password login. To administer it, drop your SSH public key on
the partition as `authorized_keys` (one key per line, the usual
`~/.ssh/id_ed25519.pub` contents). It's installed for the `admin` account,
which has passwordless `sudo`. Password authentication is disabled, so the key
is the only way in.

```sh
ssh admin@waiverserv    # or admin@<its IP>
```

## Google Drive

Put `rclone.conf` and `sa-key.json` on the partition — see [gdrive-sync.md](gdrive-sync.md).

## Security note

This partition is FAT — no permissions, readable by anyone who has the card.
The Wi-Fi password and the service account key are in the clear on it. Delete
`sa-key.json` and `wifi.txt` from the card once the kiosk has booted if that
matters; the copies on disk are what the system actually uses. Note that
removing them means a reflashed card needs configuring from scratch again.
