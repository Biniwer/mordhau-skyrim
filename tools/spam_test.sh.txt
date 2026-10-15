#!/bin/bash
# Dev: stress test - random presses of every key the mashup binds (and mouse buttons, wheel, look) into Skyrim for N
# seconds, then report whether both games are still running and what the logs say.
# usage: spam_test.sh <seconds> [seed] [menus: 1 = also Tab / F8 / console / quicksave keys]
cd /e/MordhauSkyrim
SECS=${1:-60}; SEED=${2:-1}; MENUS=${3:-0}
CMDS=$(python - "$SECS" "$SEED" "$MENUS" <<'EOF'
import random, sys
secs, seed, menus = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3] == '1'
random.seed(seed)
keys = ['0x51','0x46','0x50','0x52','0x30','0x31','0x43','0x58','0x56','0x47','0x45','0x42','0x4E','0x4D','0x4A','0x48','0x20','0x32','0x33','0x34','0x35','0x57','0x41','0x53','0x44']
holds = ['0xA2','0xA0','0x57','0x53','0x41','0x44']
if menus: keys += ['0x09','0x77','0x1B','0x0D','0x74']
out, t = [], 0
while t < secs * 1000:
    r = random.random()
    if r < 0.30: out.append('click 960 540'); t += 100
    elif r < 0.42: out.append('click 960 540 right'); t += 100
    elif r < 0.52: out.append(f'rel {random.randint(-400,400)} {random.randint(-150,150)}'); t += 20
    elif r < 0.62:
        k = random.choice(holds); d = random.randint(100, 900); out.append(f'hold {k} {d}'); t += d
    elif r < 0.66: out.append('mdown 960 540'); out.append('mup'); t += 60
    else:
        out.append('key ' + random.choice(keys)); t += 50
    if random.random() < 0.25:
        d = random.randint(20, 250); out.append(f'wait {d}'); t += d
print(';'.join(out))
EOF
)
bash tools/skyrim_keys.sh "$CMDS" >/dev/null 2>&1
sleep 2
L="$USERPROFILE/Documents/My Games/Skyrim Special Edition/SKSE/MordhauSkyrim.log"
U="/e/steam object/steamapps/common/Mordhau/Mordhau/Binaries/Win64/UE4SS.log"
echo "after ${SECS}s of spam (seed $SEED, menus $MENUS): running: $(tasklist | grep -iE 'SkyrimSE|Mordhau-Win64' | awk '{print $1}' | tr '\n' ' ')"
echo "  requests sent to MORDHAU: $(grep -c 'asking MORDHAU for' "$L"), script errors: $(grep -a -c -iE 'error|FAILED' "$U"), plugin warnings: $(grep -c -E '\[(warning|error|critical)\]' "$L")"
grep -a -iE "error|FAILED" "$U" | sed -E 's/^\[[^]]*\] //' | sort | uniq -c | sort -rn | head -5 | cut -c1-200
grep -E '\[(warning|error|critical)\]' "$L" | cut -c16- | sort | uniq -c | sort -rn | head -5 | cut -c1-200
# keep this round's plugin log (the next round clears it): the newest 12
mkdir -p /e/MordhauSkyrim/cache/logs; cp "$L" "/e/MordhauSkyrim/cache/logs/DM-$(date +%m%d-%H%M%S).log" 2>/dev/null
ls -t /e/MordhauSkyrim/cache/logs/DM-*.log 2>/dev/null | tail -n +13 | xargs -r rm -f
