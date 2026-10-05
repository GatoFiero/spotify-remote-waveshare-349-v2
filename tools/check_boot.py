"""Capture boot diagnostics without exposing local Wi-Fi or Spotify credentials."""
import re
import argparse
import json
import time
from pathlib import Path
import serial
from esptool.reset import HardReset

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port',required=True,help='USB serial port, for example COM6 or /dev/ttyACM0')
parser.add_argument('--seconds',type=float,default=18)
args = parser.parse_args()
private = [json.loads(value) for value in re.findall(r'^#define\s+(?:WIFI_SSID|WIFI_PASSWORD|SPOTIFY_CLIENT_ID|SPOTIFY_REFRESH_TOKEN)\s+(".*")$', (root/'src'/'Secrets.h').read_text(encoding='utf-8'),re.M)]
with serial.Serial(args.port,115200,timeout=.25) as port:
    port.dtr = False
    HardReset(port, uses_usb=True)()
    deadline = time.monotonic()+args.seconds
    data = bytearray()
    while time.monotonic()<deadline:
        data.extend(port.read(8192))
output = data.decode('utf-8',errors='replace')
for value in private:
    if value:
        output = output.replace(value,'[REDACTED]')
(root/'local-state').mkdir(exist_ok=True)
(root/'local-state'/'boot.log').write_text(output,encoding='utf-8')
print(output)
