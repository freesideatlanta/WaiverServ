# Google Drive sync

The `waiverserv-sync` systemd service uploads everything in
`/var/lib/waiverserv` to a Google Shared Drive with `rclone copy`. It runs
whenever a waiver is saved (path unit) and every 15 minutes as catch-up, so
outages heal themselves. Local files are never deleted; already-uploaded
files are skipped.

It authenticates as a service account — an org-owned robot identity, not tied
to any employee's Google account. One-time setup:

## 1. Create a Shared Drive

- Go to our GDrive waivers folder, under Freeside Admin -> Legal -> FSWaivers
- Open it and note the ID: the last part of the URL,
  `https://drive.google.com/drive/folders/<ID>`.

## 2. Create a service account

Follow this guide:

https://eqpsolutions.com/blog/misc-5/how-to-enable-and-grant-access-to-the-google-drive-api-9

## 3. Grant it access

In the Shared Drive: Manage members → add the service account's email
(`…@….iam.gserviceaccount.com`) as **Content manager**.

## 4. Configure the kiosk

Put the downloaded key on the kiosk's SD card as `sa-key.json`, alongside an
`rclone.conf` — see [sd-card-config.md](sd-card-config.md). On a running
kiosk you can instead write them straight to `/var/lib/waiverserv-sync`:

```sh
sudo tee /var/lib/waiverserv-sync/rclone.conf <<'EOF'
[gdrive]
type = drive
service_account_file = /var/lib/waiverserv-sync/sa-key.json
team_drive = <shared drive ID>
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
