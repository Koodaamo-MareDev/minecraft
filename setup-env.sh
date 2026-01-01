#!/bin/bash

export DEBIAN_FRONTEND=noninteractive

# Python packages
apt-get update
apt-get install -y python-is-python3 python3-pil python3-numpy apt-transport-https curl

# DevKitPro-pacman
mkdir -p /usr/local/share/keyring/
curl -fsSL https://apt.devkitpro.org/devkitpro-pub.gpg -o /usr/local/share/keyring/devkitpro-pub.gpg || { echo "Error: Failed to download devkitPro GPG key" >&2; exit 1; }
echo "deb [signed-by=/usr/local/share/keyring/devkitpro-pub.gpg] https://apt.devkitpro.org stable main" > /etc/apt/sources.list.d/devkitpro.list

apt-get update
apt-get install -y devkitpro-pacman

# Wii build dependencies
ln -sf /proc/mounts /etc/mtab
dkp-pacman -Syu --noconfirm wii-dev gamecube-tools ppc-libvorbisidec
