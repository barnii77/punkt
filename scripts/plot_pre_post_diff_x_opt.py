import matplotlib.pyplot as plt
import re
from collections import defaultdict


def parse_data(file_path):
    # Store all values as lists to handle multiple occurrences
    pre_values = defaultdict(list)
    post_values = defaultdict(list)

    with open(file_path, 'r') as file:
        for line in file:
            line = line.strip()
            if not line:
                continue

            # Extract (Pre/Post), key, and value
            match = re.match(r'\((Pre|Post)\)\s+([^:]+):\s+([\d.-]+)', line)
            if match:
                typ, key, value = match.groups()
                value = float(value)

                if typ == 'Pre':
                    pre_values[key].append(value)
                else:
                    post_values[key].append(value)

    # Calculate averaged differences
    differences = []
    valid_keys = []

    # Get all unique keys that have at least one Pre and one Post value
    all_keys = sorted(set(pre_values.keys()) & set(post_values.keys()))

    for key in all_keys:
        # Get the minimum number of pairs to avoid index errors
        num_pairs = min(len(pre_values[key]), len(post_values[key]))

        if num_pairs == 0:
            continue  # skip if no pairs

        # Calculate differences for each pair
        pair_diffs = [post_values[key][i] - pre_values[key][i]
                      for i in range(num_pairs)]


        def combine_op(diffs, num_pairs):
            # return max(map(abs, diffs))
            return sum(diffs) / num_pairs

        avg_diff = combine_op(pair_diffs, num_pairs)
        differences.append(avg_diff)
        valid_keys.append(key)

    return valid_keys, differences


def plot_differences(keys, diffs):
    plt.figure(figsize=(12, 6))
    plt.plot(range(len(keys)), diffs, marker='o', linestyle='-')
    plt.xticks(range(len(keys)), keys, rotation=90)
    plt.xlabel('Keys (sorted alphabetically)')
    plt.ylabel('Average Difference (Post - Pre)')
    plt.title('Average Difference between Post and Pre values for each key')
    plt.grid(True)
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    import sys
    if len(sys.argv) > 1:
        file_path = sys.argv[1]
    else:
        file_path = "temp/dump123.txt"  # Default file path

    keys, diffs = parse_data(file_path)
    plot_differences(keys, diffs)
