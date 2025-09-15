import subprocess, platform, re, time, datetime
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

TARGET_IP = "192.168.0.112"
INTERVAL = 1.0  # seconds

times = []
delays = []

def ping_once(ip):
    system = platform.system().lower()
    cmd = ["ping", "-n", "1", ip] if system.startswith("win") else ["ping", "-c", "1", ip]
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=4)
    out = proc.stdout.lower()
    if "unreachable" in out or "timed out" in out or proc.returncode != 0:
        return None
    if "time<" in out:
        return 0.5
    m = re.search(r"time[=<]?\s*([0-9]+(?:\.[0-9]+)?)\s*ms", out)
    if m:
        return float(m.group(1))
    return None

def update(frame):
    delay = ping_once(TARGET_IP)
    now = datetime.datetime.now().strftime("%H:%M:%S")
    times.append(now)
    delays.append(delay if delay is not None else float("nan"))

    plt.cla()
    plt.plot(times, delays, marker="o", linestyle="-", color="blue")
    plt.xticks(rotation=45, fontsize=8)
    plt.ylabel("Ping delay (ms)")
    plt.title("Live Wi-Fi Motion Detection")
    plt.tight_layout()
    plt.grid(True)

ani = FuncAnimation(plt.gcf(), update, interval=int(INTERVAL*1000))
plt.show()
