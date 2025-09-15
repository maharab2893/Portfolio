# wifi_logger.py
import subprocess, platform, time, datetime, csv, re

TARGET_IP = "192.168.0.112"   # target device IP (my phone's IP)
LOG_FILE = "wifi_log.csv"
INTERVAL = 1.0  # seconds between pings

def ping_once(ip):
    system = platform.system().lower()
    cmd = ["ping", "-n", "1", ip] if system.startswith("win") else ["ping", "-c", "1", ip]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=4)
        out = proc.stdout.lower()
        # check common failure words
        if "unreachable" in out or "timed out" in out or proc.returncode != 0:
            return None
        # handle time<1ms
        if "time<" in out:
            return 0.5
        # try to extract "time=XX ms"
        m = re.search(r"time[=<]?\s*([0-9]+(?:\.[0-9]+)?)\s*ms", out)
        if m:
            return float(m.group(1))
        # fallback: any number followed by ms
        m2 = re.search(r"([0-9]+(?:\.[0-9]+)?)\s*ms", out)
        if m2:
            return float(m2.group(1))
        return None
    except Exception:
        return None

# create CSV and header
with open(LOG_FILE, "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["timestamp", "delay_ms"])

print("Starting logger. Press Ctrl+C to stop.")
try:
    while True:
        now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        delay = ping_once(TARGET_IP)
        if delay is None:
            print(f"[{now}] Ping failed")
            row_delay = ""
        else:
            print(f"[{now}] Delay: {delay} ms")
            row_delay = delay
        with open(LOG_FILE, "a", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([now, row_delay])
        time.sleep(INTERVAL)
except KeyboardInterrupt:
    print("\nStopped by user. CSV saved as", LOG_FILE)
