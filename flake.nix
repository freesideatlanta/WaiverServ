{
  description = "WaiverServ kiosk waiver signing app";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  inputs.treefmt-nix.url = "github:numtide/treefmt-nix";
  inputs.treefmt-nix.inputs.nixpkgs.follows = "nixpkgs";

  outputs =
    {
      self,
      nixpkgs,
      treefmt-nix,
    }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (s: f nixpkgs.legacyPackages.${s});
      treefmtEval =
        pkgs:
        treefmt-nix.lib.evalModule pkgs {
          projectRootFile = "flake.nix";
          programs.nixfmt.enable = true;
          programs.ruff-format.enable = true;
          programs.ruff-check.enable = true;
        };
      pythonEnv =
        pkgs:
        pkgs.python3.withPackages (ps: [
          ps.pygobject3
          ps.pycairo
        ]);
      testInputs = pkgs: {
        nativeBuildInputs = [
          pkgs.gobject-introspection
          pkgs.wrapGAppsHook4
        ];
        buildInputs = [
          (pkgs.python3.withPackages (ps: [
            ps.pygobject3
            ps.pycairo
            ps.pytest
          ]))
          pkgs.gtk4
          pkgs.xvfb-run
          pkgs.dbus
        ];
      };
    in
    {
      packages = forAllSystems (pkgs: {
        default = pkgs.stdenv.mkDerivation {
          pname = "waiverserv";
          version = "0.1.0";
          src = ./.;

          nativeBuildInputs = [
            pkgs.wrapGAppsHook4
            pkgs.gobject-introspection
          ];
          buildInputs = [
            (pythonEnv pkgs)
            pkgs.gtk4
          ];

          installPhase = ''
            runHook preInstall
            mkdir -p $out/share/waiverserv $out/bin
            cp main.py topaz.py FS_Waiver_Apr_2019.png $out/share/waiverserv/
            makeWrapper ${pythonEnv pkgs}/bin/python3 $out/bin/waiverserv \
              --add-flags $out/share/waiverserv/main.py
            runHook postInstall
          '';
        };
      });

      formatter = forAllSystems (pkgs: (treefmtEval pkgs).config.build.wrapper);

      checks = forAllSystems (pkgs: {
        formatting = (treefmtEval pkgs).config.build.check self;

        e2e = pkgs.stdenv.mkDerivation (
          (testInputs pkgs)
          // {
            name = "waiverserv-e2e";
            src = ./.;

            FONTCONFIG_FILE = pkgs.makeFontsConf { fontDirectories = [ pkgs.dejavu_fonts ]; };

            dontWrapGApps = true;
            buildPhase = ''
              runHook preBuild
              export HOME=$TMPDIR
              export XDG_DATA_DIRS=$GSETTINGS_SCHEMAS_PATH''${XDG_DATA_DIRS:+:$XDG_DATA_DIRS}
              xvfb-run -n 87 -s "-screen 0 1280x1024x24" \
                dbus-run-session --config-file=${pkgs.dbus}/share/dbus-1/session.conf -- \
                env GDK_BACKEND=x11 GSK_RENDERER=cairo python -m pytest -v
              runHook postBuild
            '';
            installPhase = "touch $out";
          }
        );

        polkit = pkgs.testers.runNixOSTest {
          name = "waiverserv-polkit";
          nodes.machine.imports = [ self.nixosModules.default ];
          testScript = ''
            start_all()
            machine.wait_for_unit("multi-user.target")

            def started():
                return machine.succeed(
                    "systemctl show waiverserv-sync.service "
                    "-p ExecMainStartTimestampMonotonic --value").strip()

            def start(unit):
                return ("su waiver -s /bin/sh -c 'busctl call org.freedesktop.systemd1 "
                        "/org/freedesktop/systemd1 org.freedesktop.systemd1.Manager "
                        "StartUnit ss {} replace'".format(unit))

            assert started() == "0", "sync ran before we asked it to"

            machine.succeed(start("waiverserv-sync.service"))
            machine.wait_until_succeeds(
                "test $(systemctl show waiverserv-sync.service "
                "-p ExecMainStartTimestampMonotonic --value) != 0")

            machine.fail(start("sshd.service"))
          '';
        };
      });

      nixosModules.default = { pkgs, ... }: {
        security.polkit.enable = true;
        security.polkit.extraConfig = ''
          polkit.addRule(function(action, subject) {
            if (action.id == "org.freedesktop.systemd1.manage-units" &&
                action.lookup("unit") == "waiverserv-sync.service" &&
                action.lookup("verb") == "start" &&
                subject.user == "waiver") {
              return polkit.Result.YES;
            }
          });
        '';
        users.users.waiver = {
          isNormalUser = true;
          group = "waiver";
        };
        users.groups.waiver = { };

        services.udev.extraRules = ''
          KERNEL=="hidraw*", ATTRS{idVendor}=="06a8", ATTRS{idProduct}=="0043", GROUP="waiver", MODE="0660", SYMLINK+="topaz"
        '';
        systemd.tmpfiles.rules = [
          "d /var/lib/waiverserv 0750 waiver waiver -"
          "d /var/lib/waiverserv-sync 0700 waiver waiver -"
        ];

        systemd.services.waiverserv-sync = {
          description = "Upload signed waivers to Google Drive";
          serviceConfig = {
            Type = "oneshot";
            User = "waiver";
            ExecStart = "${pkgs.rclone}/bin/rclone copy /var/lib/waiverserv gdrive: --config /var/lib/waiverserv-sync/rclone.conf --exclude '*.tmp'";
          };
        };
        systemd.timers.waiverserv-sync = {
          wantedBy = [ "timers.target" ];
          timerConfig = {
            OnBootSec = "5min";
            OnUnitActiveSec = "15min";
          };
        };
      };

      nixosConfigurations.waiverserv = nixpkgs.lib.nixosSystem {
        system = "aarch64-linux";
        modules = [
          "${nixpkgs}/nixos/modules/installer/sd-card/sd-image-aarch64.nix"
          self.nixosModules.default
          (
            { pkgs, lib, ... }:
            let
              app = self.packages.${pkgs.stdenv.hostPlatform.system}.default;

              # A 4K framebuffer doesn't fit in CMA, so pick the largest mode
              # under a pixel budget, and rotate landscape panels into portrait.
              pickOutput = pkgs.writeText "waiverserv-pick-output.py" ''
                import json, os, subprocess, sys

                WLR_RANDR = "${pkgs.wlr-randr}/bin/wlr-randr"
                MAX_PIXELS = int(os.environ.get("WAIVER_MAX_PIXELS", 1920 * 1200))

                heads = json.loads(subprocess.check_output([WLR_RANDR, "--json"]))
                if not heads:
                    sys.exit("no outputs connected")
                out = heads[0]

                px = lambda m: m["width"] * m["height"]
                fits = [m for m in out["modes"] if px(m) <= MAX_PIXELS]
                best = max(fits, key=px) if fits else min(out["modes"], key=px)

                # Panels report landscape modes even when physically rotated.
                transform = "270" if best["width"] > best["height"] else "180"

                subprocess.check_call(
                    [WLR_RANDR, "--output", out["name"], "--on",
                     "--mode", "%dx%d" % (best["width"], best["height"]),
                     "--transform", transform]
                    + sum([["--output", h["name"], "--off"] for h in heads[1:]], []))
              '';

              kiosk = pkgs.writeShellScript "waiverserv-kiosk" ''
                ${pkgs.python3}/bin/python3 ${pickOutput} || true
                ${app}/bin/waiverserv
                ${pkgs.sway}/bin/swaymsg exit
              '';

              swayConfig = pkgs.writeText "waiverserv-sway.conf" ''
                output * bg #000000 solid_color
                seat * hide_cursor 1
                exec ${kiosk}
              '';
            in
            {
              disabledModules = [ "profiles/base.nix" ];
              networking.hostName = "waiverserv";
              system.stateVersion = "26.11";

              sdImage.populateFirmwareCommands = lib.mkAfter ''
                chmod +w firmware/config.txt
                printf '\n[all]\ndtoverlay=vc4-kms-v3d\n' >> firmware/config.txt
              '';

              # extlinux boots the kernel's own dtbs (FDTDIR), so config.txt
              # overlay params like cma-512 never reach the kernel -- CMA has to
              # come from the command line. 64 MiB (the default) is not enough
              # for double-buffered scanout at anything above 1080p.
              boot.kernelParams = [ "cma=128M" ];

              hardware.graphics.enable = true;

              systemd.services.waiverserv-kiosk = {
                description = "WaiverServ kiosk session";
                after = [
                  "systemd-user-sessions.service"
                  "systemd-logind.service"
                  "getty@tty1.service"
                ];
                before = [ "graphical.target" ];
                wants = [
                  "dbus.socket"
                  "systemd-logind.service"
                ];
                wantedBy = [ "graphical.target" ];
                conflicts = [ "getty@tty1.service" ];
                unitConfig.ConditionPathExists = "/dev/tty1";
                environment = {
                  WAIVER_SAVE_DIR = "/var/lib/waiverserv";
                  # GTK's Vulkan renderer segfaults instead of failing gracefully
                  # when v3dv can't get CMA for a swapchain.
                  GSK_RENDERER = "gl";
                };
                serviceConfig = {
                  ExecStart = "${pkgs.sway}/bin/sway -c ${swayConfig}";
                  User = "waiver";
                  Restart = "always";
                  RestartSec = 2;
                  IgnoreSIGPIPE = "no";
                  # replaces (a)getty on tty1, so log the user in utmp
                  UtmpIdentifier = "%n";
                  UtmpMode = "user";
                  TTYPath = "/dev/tty1";
                  TTYReset = "yes";
                  TTYVHangup = "yes";
                  TTYVTDisallocate = "yes";
                  StandardInput = "tty-fail";
                  StandardOutput = "journal";
                  StandardError = "journal";
                  PAMName = "waiverserv-kiosk";
                };
              };
              security.pam.services.waiverserv-kiosk.startSession = true;
              systemd.defaultUnit = "graphical.target";

              users.users.admin = {
                isNormalUser = true;
                extraGroups = [ "wheel" ];
              };
              security.sudo.extraRules = [
                {
                  users = [ "admin" ];
                  commands = [
                    {
                      command = "ALL";
                      options = [ "NOPASSWD" ];
                    }
                  ];
                }
              ];
              services.openssh = {
                enable = true;
                settings.PasswordAuthentication = false;
              };
              time.timeZone = "America/New_York";

              networking.useNetworkd = true;
              services.resolved = {
                enable = true;
                settings.Resolve.MulticastDNS = "yes";
              };
              systemd.network.networks."99-wireless-client-dhcp".networkConfig.MulticastDNS = true;
              networking.firewall.allowedUDPPorts = [ 5353 ];

              nix.settings.trusted-users = [ "admin" ];
              nix.settings.experimental-features = [
                "nix-command"
                "flakes"
              ];

              environment.systemPackages = [ pkgs.btop ];
              programs.neovim = {
                enable = true;
                defaultEditor = true;
                vimAlias = true;
                viAlias = true;
              };

              hardware.enableRedistributableFirmware = lib.mkForce false;
              hardware.firmware = [ pkgs.raspberrypiWirelessFirmware ];
              networking.wireless.enable = true;

              systemd.services.waiverserv-provision = {
                description = "Load Wi-Fi and rclone config from the SD card boot partition";
                wantedBy = [ "multi-user.target" ];
                after = [ "boot-firmware.mount" ];
                requires = [ "boot-firmware.mount" ];
                before = [
                  "wpa_supplicant.service"
                  "waiverserv-sync.service"
                  "sshd.service"
                ];
                serviceConfig.Type = "oneshot";
                serviceConfig.RemainAfterExit = true;
                script = ''
                  if [ -f /boot/firmware/wifi.txt ]; then
                    ssid=$(${pkgs.gnused}/bin/sed -n 1p /boot/firmware/wifi.txt | ${pkgs.coreutils}/bin/tr -d '\r')
                    psk=$(${pkgs.gnused}/bin/sed -n 2p /boot/firmware/wifi.txt | ${pkgs.coreutils}/bin/tr -d '\r')
                    mkdir -p /etc/wpa_supplicant
                    if [ -n "$psk" ]; then
                      ${pkgs.wpa_supplicant}/bin/wpa_passphrase "$ssid" "$psk" > /etc/wpa_supplicant/imperative.conf
                    else
                      printf 'network={\n\tssid="%s"\n\tkey_mgmt=NONE\n}\n' "$ssid" > /etc/wpa_supplicant/imperative.conf
                    fi
                    chmod 600 /etc/wpa_supplicant/imperative.conf
                  fi

                  for f in rclone.conf sa-key.json; do
                    if [ -f "/boot/firmware/$f" ]; then
                      install -o waiver -g waiver -m 600 "/boot/firmware/$f" "/var/lib/waiverserv-sync/$f"
                    fi
                  done

                  if [ -f /boot/firmware/authorized_keys ]; then
                    install -d -o admin -g users -m 700 /home/admin/.ssh
                    install -o admin -g users -m 600 /boot/firmware/authorized_keys /home/admin/.ssh/authorized_keys
                  fi
                '';
              };

              # overall closure size is 2.9GiB. of that, 700MiB is mesa/llvm.
              # trimming is possible, but requires a slow ARM rebuild -> not worth
            }
          )
        ];
      };

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell (testInputs pkgs);
      });
    };
}
