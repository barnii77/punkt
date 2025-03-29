import re
import math
import matplotlib.pyplot as plt

# Specify the input filename (ensure the file exists in the working directory)
filename = 'temp/abx2.txt'

# Read data from the file
with open(filename, 'r') as file:
    data_text = file.read()

# Use regex to extract rank and x-value from each line
pattern = r"Average barycenter X of rank (\d+): (-?[\d.]+)"
matches = re.findall(pattern, data_text)

# Create a dictionary to store (time, x) data for each rank.
# We use the order of appearance as a proxy for time.
data = {}
time_counter = {}

for rank_str, x_str in matches:
    rank = int(rank_str)
    x = float(x_str)
    x_val = x / abs(x) * math.log10(abs(x) + 1)
    # Create a time stamp: increment a counter for each rank
    if rank not in time_counter:
        time_counter[rank] = 0
        data[rank] = []
    data[rank].append((time_counter[rank], x_val))
    time_counter[rank] += 1

plt.figure(figsize=(12, 7))

# Plot each rank's data
for rank, points in data.items():
    # Unpack time and x values
    times, x_values = zip(*points)
    plt.plot(times, x_values, marker='o', linestyle='-', label=f"Rank {rank}")

plt.xlabel("Time (sequential index)")
plt.ylabel("Barycenter X as sign(x) * log10(abs(x) + 1)")
plt.title("Barycenter X vs Time for Different Ranks")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.show()
