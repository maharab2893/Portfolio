# wifi_plot.py
import pandas as pd
import matplotlib.pyplot as plt

CSV_FILE = "wifi_log.csv"
df = pd.read_csv(CSV_FILE, parse_dates=["timestamp"])

# convert empty cells to NaN, then fill or drop
df["delay_ms"] = pd.to_numeric(df["delay_ms"], errors="coerce")

plt.figure(figsize=(12,5))
plt.plot(df["timestamp"], df["delay_ms"], marker='o', linestyle='-')
plt.xlabel("Time")
plt.ylabel("Ping delay (ms)")
plt.title("Wi-Fi Motion Detection (ping delay)")
plt.grid(True)
plt.xticks(rotation=45)
plt.tight_layout()
plt.show()
