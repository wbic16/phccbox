#!/bin/bash
# Create the stub bin directory: one symlink per stubbed command.
set -e
D="${1:-$HOME/phccbox-work/exo-test/stubbin}"
mkdir -p "$D"
cp "$(dirname "$0")/stub.sh" "$D/.stub-real.sh"
chmod +x "$D/.stub-real.sh"
for n in sudo date hostname ping avahi-browse systemctl ollama curl wget git \
         squidclient top du lspci ufw exportfs mountpoint ip go gpg sleep \
         apt apt-get npm rsync ssh killall modprobe rmmod swapoff swapon mkswap \
         zramctl growpart lsblk lvextend resize2fs pvs vgs mount snap service \
         rustup cargo python3 ssh-keygen openclaw make apt-key dpkg \
         add-apt-repository apt-mark aa-complain apparmor_parser \
         update-ca-certificates update-grub sysctl journalctl chown squid; do
  ln -sf .stub-real.sh "$D/$n"
done
echo "stubbin ready: $(ls "$D" | wc -l) entries"
