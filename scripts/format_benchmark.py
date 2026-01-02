#!/usr/bin/env python3
"""
ServerLink Benchmark Result Formatter

Parses benchmark output files and formats them into JSON and Markdown.
"""

import json
import re
import sys
from pathlib import Path
from typing import Dict, List, Any, Optional
from datetime import datetime, timezone


def parse_throughput_benchmark(content: str) -> Dict[str, Any]:
    """Parse throughput benchmark output."""
    result = {
        "type": "throughput",
        "metrics": {}
    }

    # Example patterns from bench_throughput output:
    # Message size: 1024 bytes
    # Messages: 100000
    # Elapsed time: 1234.56 ms
    # Throughput: 81000.00 msg/s
    # Bandwidth: 82.94 MB/s

    patterns = {
        "message_size": r"Message size:\s*(\d+)\s*bytes",
        "message_count": r"Messages:\s*(\d+)",
        "elapsed_ms": r"Elapsed time:\s*([\d.]+)\s*ms",
        "throughput_msg_per_sec": r"Throughput:\s*([\d.]+)\s*msg/s",
        "bandwidth_mb_per_sec": r"Bandwidth:\s*([\d.]+)\s*MB/s"
    }

    for key, pattern in patterns.items():
        match = re.search(pattern, content)
        if match:
            value = match.group(1)
            result["metrics"][key] = float(value) if '.' in value else int(value)

    return result


def parse_latency_benchmark(content: str) -> Dict[str, Any]:
    """Parse latency benchmark output."""
    result = {
        "type": "latency",
        "metrics": {}
    }

    # Example patterns from bench_latency output:
    # Round-trip latency test
    # Message size: 1024 bytes
    # Round-trips: 10000
    # Average latency: 12.34 us
    # Min latency: 8.50 us
    # Max latency: 45.67 us

    patterns = {
        "message_size": r"Message size:\s*(\d+)\s*bytes",
        "roundtrips": r"Round-trips:\s*(\d+)",
        "avg_latency_us": r"Average latency:\s*([\d.]+)\s*us",
        "min_latency_us": r"Min latency:\s*([\d.]+)\s*us",
        "max_latency_us": r"Max latency:\s*([\d.]+)\s*us"
    }

    for key, pattern in patterns.items():
        match = re.search(pattern, content)
        if match:
            value = match.group(1)
            result["metrics"][key] = float(value) if '.' in value else int(value)

    return result


def parse_pubsub_benchmark(content: str) -> Dict[str, Any]:
    """Parse pub/sub benchmark output."""
    result = {
        "type": "pubsub",
        "metrics": {}
    }

    # Example patterns from bench_pubsub output:
    # Publishers: 1
    # Subscribers: 10
    # Messages per publisher: 10000
    # Message size: 1024 bytes
    # Total messages: 100000
    # Elapsed time: 2345.67 ms
    # Throughput: 42650.00 msg/s

    patterns = {
        "publishers": r"Publishers:\s*(\d+)",
        "subscribers": r"Subscribers:\s*(\d+)",
        "messages_per_pub": r"Messages per publisher:\s*(\d+)",
        "message_size": r"Message size:\s*(\d+)\s*bytes",
        "total_messages": r"Total messages:\s*(\d+)",
        "elapsed_ms": r"Elapsed time:\s*([\d.]+)\s*ms",
        "throughput_msg_per_sec": r"Throughput:\s*([\d.]+)\s*msg/s"
    }

    for key, pattern in patterns.items():
        match = re.search(pattern, content)
        if match:
            value = match.group(1)
            result["metrics"][key] = float(value) if '.' in value else int(value)

    return result


def parse_benchmark_file(filepath: Path) -> Optional[Dict[str, Any]]:
    """Parse a single benchmark output file."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading {filepath}: {e}", file=sys.stderr)
        return None

    name = filepath.stem.replace('.txt', '')

    # Determine benchmark type and parse
    if 'throughput' in name.lower():
        parsed = parse_throughput_benchmark(content)
    elif 'latency' in name.lower():
        parsed = parse_latency_benchmark(content)
    elif 'pubsub' in name.lower():
        parsed = parse_pubsub_benchmark(content)
    else:
        # Generic parser
        parsed = {
            "type": "unknown",
            "metrics": {},
            "raw_output": content
        }

    parsed["name"] = name
    return parsed


def format_as_json(benchmarks: List[Dict[str, Any]], output_file: Path):
    """Format benchmarks as JSON."""
    output = {
        "timestamp": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "platform": sys.platform,
        "benchmarks": benchmarks
    }

    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(output, f, indent=2)


def format_as_markdown(benchmarks: List[Dict[str, Any]], output_file: Path):
    """Format benchmarks as Markdown table."""
    lines = [
        "# ServerLink Benchmark Results",
        "",
        f"**Date:** {datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M:%S UTC')}  ",
        f"**Platform:** {sys.platform}",
        "",
        "## Summary",
        "",
        "| Benchmark | Type | Key Metric | Value |",
        "|-----------|------|------------|-------|"
    ]

    for bench in benchmarks:
        name = bench.get("name", "Unknown")
        bench_type = bench.get("type", "unknown")
        metrics = bench.get("metrics", {})

        # Determine key metric
        if bench_type == "throughput":
            key_metric = "Throughput"
            value = f"{metrics.get('throughput_msg_per_sec', 0):.2f} msg/s"
        elif bench_type == "latency":
            key_metric = "Avg Latency"
            value = f"{metrics.get('avg_latency_us', 0):.2f} µs"
        elif bench_type == "pubsub":
            key_metric = "Throughput"
            value = f"{metrics.get('throughput_msg_per_sec', 0):.2f} msg/s"
        else:
            key_metric = "N/A"
            value = "N/A"

        lines.append(f"| {name} | {bench_type} | {key_metric} | {value} |")

    lines.extend(["", "## Detailed Results", ""])

    # Add detailed sections for each benchmark
    for bench in benchmarks:
        name = bench.get("name", "Unknown")
        metrics = bench.get("metrics", {})

        lines.extend([
            f"### {name}",
            "",
            "```"
        ])

        for key, value in metrics.items():
            if isinstance(value, float):
                lines.append(f"{key}: {value:.2f}")
            else:
                lines.append(f"{key}: {value}")

        lines.extend(["```", ""])

    with open(output_file, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))


def main():
    if len(sys.argv) < 3:
        print("Usage: format_benchmark.py <input_dir> <output_file>", file=sys.stderr)
        sys.exit(1)

    input_dir = Path(sys.argv[1])
    output_file = Path(sys.argv[2])

    if not input_dir.is_dir():
        print(f"Error: Input directory not found: {input_dir}", file=sys.stderr)
        sys.exit(1)

    # Find all benchmark result files
    result_files = list(input_dir.glob("*.txt"))

    if not result_files:
        print(f"Warning: No .txt files found in {input_dir}", file=sys.stderr)
        sys.exit(0)

    # Parse all benchmarks
    benchmarks = []
    for result_file in sorted(result_files):
        parsed = parse_benchmark_file(result_file)
        if parsed:
            benchmarks.append(parsed)

    if not benchmarks:
        print("Warning: No benchmarks could be parsed", file=sys.stderr)
        sys.exit(0)

    # Determine output format from extension
    if output_file.suffix.lower() == '.json':
        format_as_json(benchmarks, output_file)
        print(f"JSON results written to: {output_file}")
    elif output_file.suffix.lower() == '.md':
        format_as_markdown(benchmarks, output_file)
        print(f"Markdown results written to: {output_file}")
    else:
        # Default to JSON
        format_as_json(benchmarks, output_file)
        print(f"JSON results written to: {output_file}")

    print(f"Processed {len(benchmarks)} benchmark(s)")


if __name__ == "__main__":
    main()
