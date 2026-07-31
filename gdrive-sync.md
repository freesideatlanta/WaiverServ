# Google Drive sync

The `waiverserv-sync` systemd service uploads everything in
`/var/lib/waiverserv` to a Google Shared Drive with `rclone copy`. It runs
whenever a waiver is saved (path unit) and every 15 minutes as catch-up, so
outages heal themselves. Local files are never deleted; already-uploaded
files are skipped.

It authenticates as a service account — an org-owned robot identity, not tied
to any employee's Google account. One-time setup:

## 1. Note the two IDs

Our waivers folder is Freeside Admin -> Legal -> FSWaivers. You need both the
Shared Drive it lives in and the folder itself; they are different IDs and
rclone wants both.

- **Shared drive ID** — click the Shared Drive in the left sidebar. The URL is
  `https://drive.google.com/drive/folders/<ID>`, and the ID starts with `0A`
  (~19 characters).
- **Folder ID** — open FSWaivers. Same URL shape, but the ID starts with `1`
  (~33 characters).

Putting the folder ID in `team_drive` is the easy mistake; rclone reports it
as `Error 404: Shared drive not found`.

## 2. Create a service account

Follow this guide:

https://eqpsolutions.com/blog/misc-5/how-to-enable-and-grant-access-to-the-google-drive-api-9

## 3. Grant it access

In the Shared Drive: Manage members → add the service account's email
(`…@….iam.gserviceaccount.com`) as **Content manager**.

A service account that isn't a member gets the same `notFound` error as a
wrong ID, so do this before debugging anything else.

## 4. Configure the kiosk

Put the downloaded key on the kiosk's SD card as `sa-key.json`, alongside an
`rclone.conf` — see [sd-card-config.md](sd-card-config.md); both are copied to
`/var/lib/waiverserv-sync/` at boot, so `service_account_file` must point
there rather than at the card. On a running
kiosk you can instead write them straight to `/var/lib/waiverserv-sync`:

```sh
sudo tee /var/lib/waiverserv-sync/rclone.conf <<'EOF'
[gdrive]
type = drive
service_account_file = /var/lib/waiverserv-sync/sa-key.json
team_drive = <shared drive ID, 0A…>
root_folder_id = <FSWaivers folder ID, 1…>
EOF
sudo chown waiver:waiver /var/lib/waiverserv-sync/*
sudo chmod 600 /var/lib/waiverserv-sync/*
```

## 5. Test

```sh
sudo systemctl start waiverserv-sync
journalctl -u waiverserv-sync
```

Existing waivers should appear in the Shared Drive.

## Key rotation

Generate a new JSON key in the Cloud console (step 2), replace
`sa-key.json`, delete the old key. Nothing else changes.
