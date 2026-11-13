#!/bin/sh
# Builds the Onion OS app folder. Run after `make miyoo` produced ./abobo.  Needs ffmpeg for the sound conversion.
set -e
OUT=AbobosBigAdventure
rm -rf $OUT; mkdir -p $OUT/data/sounds
cp abobo $OUT/abobo
cp assets/scene.bin assets/bitmaps.pak $OUT/data/
mkdir -p /tmp/abobo_snd && tar -xf assets/sounds.tar -C /tmp/abobo_snd
for f in /tmp/abobo_snd/sounds/*.mp3; do b=$(basename "$f" .mp3); id=${b%%_*}; ffmpeg -loglevel error -y -i "$f" -ar 22050 -ac 1 -c:a pcm_s16le "$OUT/data/sounds/$id.wav"; done
cp onion/icon.png $OUT/icon.png
cat > $OUT/launch.sh <<'EOS'
#!/bin/sh
cd "$(dirname "$0")"
chmod +x ./abobo
./abobo ./data
EOS
chmod +x $OUT/launch.sh
cat > $OUT/config.json <<'EOC'
{ "label": "Abobo's Big Adventure", "icon": "icon.png", "iconsel": "icon.png", "launch": "launch.sh", "description": "Native Onion OS port" }
EOC
echo "Copy the $OUT folder to /mnt/SDCARD/App/ on the Miyoo SD card."
