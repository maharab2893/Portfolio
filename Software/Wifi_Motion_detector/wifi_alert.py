# wifi_alert.py
import pandas as pd
CSV_FILE = "wifi_log.csv"
THRESHOLD_MIN = 5.0  # minimum ms threshold

df = pd.read_csv(CSV_FILE, parse_dates=["timestamp"])
df["delay_ms"] = pd.to_numeric(df["delay_ms"], errors="coerce")
df = df.dropna(subset=["delay_ms"]).reset_index(drop=True)

if len(df) < 2:
    print("Not enough numeric data. Run logger longer.")
    exit()

mean = df["delay_ms"].mean()
std = df["delay_ms"].std()
threshold = max(THRESHOLD_MIN, mean + 3*std)
print(f"Suggested threshold = {threshold:.2f} ms  (mean={mean:.2f}, std={std:.2f})\n")

for i in range(1, len(df)):
    change = abs(df.loc[i, "delay_ms"] - df.loc[i-1, "delay_ms"])
    if change > threshold:
        print(f"Movement detected at {df.loc[i, 'timestamp']} (change={change:.2f} ms)")
