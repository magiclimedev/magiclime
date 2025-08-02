#!/usr/bin/env python3
import sys
import json
import csv
import argparse
from datetime import datetime

# Parse command line arguments
parser = argparse.ArgumentParser()
parser.add_argument('-verbose', action='store_true', help='Show all readings including repeater signals')
args = parser.parse_args()

# Read the mapping from identify.csv
name_map = {}
with open('identify.csv', 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        name_map[row['uid'].strip('"')] = row['name'].strip('"')

# Keep track of readings by UID
readings = {}

def parse_timestamp(ts_str):
    return datetime.strptime(ts_str, "%Y-%m-%d %H:%M:%S")

# Buffer to collect readings before printing
buffer = []

# Process each line from stdin
for line in sys.stdin:
    line = line.strip()
    if not line:  # Skip empty lines
        continue
    try:
        # Try to parse as JSON
        data = json.loads(line)
        # Skip non-sensor data
        if data.get('source') == 'rx':
            print(json.dumps(data))
            continue
            
        # Only process lines that have required fields
        if all(k in data for k in ['uid', 'timestamp', 'source', 'rss']):
            uid = data['uid']
            timestamp = parse_timestamp(data['timestamp'])
            rss = int(data.get('rss', -999))
            
            # Add name mapping if available
            if uid in name_map:
                data['uid'] = f"{uid} ({name_map[uid]})"
            
            # In verbose mode, print all readings
            if args.verbose:
                print(json.dumps(data))
                continue

            # Add to buffer
            buffer.append({'data': data, 'timestamp': timestamp, 'rss': rss})
            
            # Process buffer when we have readings or on a different timestamp
            if len(buffer) > 1:
                last = buffer[-1]
                prev = buffer[-2]
                time_diff = abs((last['timestamp'] - prev['timestamp']).total_seconds())
                
                # If timestamps are more than 1 second apart
                if time_diff > 1:
                    # Print the best signal from previous timestamp
                    print(json.dumps(prev['data']))
                    buffer = [last]  # Keep only the latest reading
                # If timestamps are within 1 second and we have both readings
                elif time_diff <= 1 and last['data']['uid'] == prev['data']['uid']:
                    # Keep only the stronger signal
                    if last['rss'] > prev['rss']:
                        buffer = [last]
                    else:
                        buffer = [prev]
        else:
            # Print non-sensor data lines as-is
            print(line)
            
        # Print any buffered readings if we hit a non-sensor line
        if buffer and not all(k in data for k in ['uid', 'timestamp', 'source', 'rss']):
            print(json.dumps(buffer[0]['data']))
            buffer = []
            
    except json.JSONDecodeError:
        # Print non-JSON lines as-is
        print(line)
        # Print any buffered readings if we hit a non-JSON line
        if buffer:
            print(json.dumps(buffer[0]['data']))
            buffer = []
    except ValueError as e:
        # Handle timestamp parsing errors
        print(line)

# Print any remaining buffered reading
if buffer:
    print(json.dumps(buffer[0]['data']))