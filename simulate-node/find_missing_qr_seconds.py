import re
from datetime import datetime, timedelta

# Adjust this to your actual log file path
log_file = "your_log_file.log"

# Regex to find ISO timestamps
timestamp_regex = re.compile(r'"timestamp":"(.*?)"')

seen_seconds = set()

with open(log_file, "r") as f:
    for line in f:
        match = timestamp_regex.search(line)
        if match:
            timestamp = datetime.fromisoformat(match.group(1).replace("Z", "+00:00"))
            seen_seconds.add(timestamp.replace(microsecond=0))  # Truncate to second

if not seen_seconds:
    print("No valid timestamps found.")
    exit(1)

# Get bounds
start = min(seen_seconds)
end = max(seen_seconds)

# Generate all seconds in the range
all_seconds = set(start + timedelta(seconds=i) for i in range(int((end - start).total_seconds()) + 1))

# Find missing seconds
missing_seconds = sorted(all_seconds - seen_seconds)

print(f"Total duration: {start} to {end}")
print(f"Missing seconds ({len(missing_seconds)} total):")
for ts in missing_seconds:
    print(ts)
