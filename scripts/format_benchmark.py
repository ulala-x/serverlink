#!/usr/bin/env python3
"""
ServerLink Benchmark Result Formatter

Parses benchmark output files and formats them into JSON and Markdown.
"""

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Dict, List, Any, Optional
from datetime import datetime, timezone


def parse_table_line(line: str, columns: List[str]) -> Optional[Dict[str, str]]:
    """Parse a single table line with pipe-delimited columns."""
    # Split by pipe and strip whitespace
    parts = [p.strip() for p in line.split('|')]

    # Filter out empty parts (from leading/trailing pipes)
    parts = [p for p in parts if p]

    if len(parts) != len(columns):
        return None

    # Build result dictionary
    result = {}
    for col, value in zip(columns, parts):
        result[col] = value

    return result


def extract_number(value_str: str) -> Optional[float]:
    """Extract numeric value from a string like '1024 bytes' or '50000 msg/s'."""
    # Remove commas for easier parsing
    value_str = value_str.replace(',', '')

    # For percentile format like "p50: 56.13 us", extract the value after colon
    if ':' in value_str:
        # Split by colon and take the part after it
        parts = value_str.split(':')
        if len(parts) >= 2:
            value_str = parts[1].strip()

    # Match patterns like: "1024 bytes", "50.23 ms", "4921169 msg/s"
    match = re.search(r'([\d.]+)', value_str)
    if match:
        num_str = match.group(1)
        return float(num_str) if '.' in num_str else int(num_str)
    return None


def parse_throughput_benchmark(content: str) -> Dict[str, Any]:
    """Parse throughput benchmark output (table format)."""
    result = {
        "type": "throughput",
        "results": []
    }

    # Table format:
    # Transport            |   Message Size | Message Count |        Time |     Throughput |    Bandwidth
    # TCP                  |       64 bytes |   100000 msgs |    20.32 ms |    4921169 msg/s |   300.36 MB/s

    columns = ["Transport", "Message Size", "Message Count", "Time", "Throughput", "Bandwidth"]

    lines = content.split('\n')
    for line in lines:
        if '|' not in line or 'Transport' in line or '---' in line:
            continue

        parsed = parse_table_line(line, columns)
        if not parsed:
            continue

        # Extract numeric values
        entry = {
            "transport": parsed["Transport"],
            "message_size": extract_number(parsed["Message Size"]),
            "message_count": extract_number(parsed["Message Count"]),
            "elapsed_ms": extract_number(parsed["Time"]),
            "throughput_msg_per_sec": extract_number(parsed["Throughput"]),
            "bandwidth_mb_per_sec": extract_number(parsed["Bandwidth"])
        }

        # Only add if we got valid numeric data
        if all(v is not None for k, v in entry.items() if k != "transport"):
            result["results"].append(entry)

    # Fallback to simple key-value format if table parsing failed
    if not result["results"]:
        fallback = parse_simple_format(content, {
            "message_size": r"Message size:\s*(\d+)\s*bytes",
            "message_count": r"Messages:\s*(\d+)",
            "elapsed_ms": r"Elapsed time:\s*([\d.]+)\s*ms",
            "throughput_msg_per_sec": r"Throughput:\s*([\d.]+)\s*msg/s",
            "bandwidth_mb_per_sec": r"Bandwidth:\s*([\d.]+)\s*MB/s"
        })
        if fallback:
            result["results"] = [fallback]

    return result


def parse_simple_format(content: str, patterns: Dict[str, str]) -> Optional[Dict[str, Any]]:
    """Parse simple key-value format as fallback."""
    result = {}
    for key, pattern in patterns.items():
        match = re.search(pattern, content)
        if match:
            value = match.group(1)
            result[key] = float(value) if '.' in value else int(value)
    return result if result else None


def parse_latency_benchmark(content: str) -> Dict[str, Any]:
    """Parse latency benchmark output (table format)."""
    result = {
        "type": "latency",
        "results": []
    }

    # Table format:
    # Transport            |   Message Size |      Average |            p50 |            p95 |            p99
    # TCP                  |       64 bytes | avg:    73.13 us | p50:    56.13 us | p95:   129.16 us | p99:   232.59 us

    columns = ["Transport", "Message Size", "Average", "p50", "p95", "p99"]

    lines = content.split('\n')
    for line in lines:
        if '|' not in line or 'Transport' in line or '---' in line:
            continue

        parsed = parse_table_line(line, columns)
        if not parsed:
            continue

        # Extract numeric values (handle "avg: 73.13 us" format)
        entry = {
            "transport": parsed["Transport"],
            "message_size": extract_number(parsed["Message Size"]),
            "avg_latency_us": extract_number(parsed["Average"]),
            "p50_latency_us": extract_number(parsed["p50"]),
            "p95_latency_us": extract_number(parsed["p95"]),
            "p99_latency_us": extract_number(parsed["p99"])
        }

        # Only add if we got valid numeric data
        if all(v is not None for k, v in entry.items() if k != "transport"):
            result["results"].append(entry)

    # Fallback to simple key-value format if table parsing failed
    if not result["results"]:
        fallback = parse_simple_format(content, {
            "message_size": r"Message size:\s*(\d+)\s*bytes",
            "roundtrips": r"Round-trips:\s*(\d+)",
            "avg_latency_us": r"Average latency:\s*([\d.]+)\s*us",
            "min_latency_us": r"Min latency:\s*([\d.]+)\s*us",
            "max_latency_us": r"Max latency:\s*([\d.]+)\s*us"
        })
        if fallback:
            result["results"] = [fallback]

    return result


def parse_pubsub_benchmark(content: str) -> Dict[str, Any]:
    """Parse pub/sub benchmark output (table format)."""
    result = {
        "type": "pubsub",
        "results": []
    }

    # Table format varies depending on test:
    # Single publisher:
    # Transport            |   Message Size | Message Count |        Time |     Throughput |    Bandwidth
    # Multiple subscribers:
    # Transport            |   Subs |   Message Size | Total Msgs |     Throughput |    Bandwidth

    lines = content.split('\n')

    # Detect column structure from header
    header_line = None
    for line in lines:
        if '|' in line and 'Transport' in line:
            header_line = line
            break

    if not header_line:
        # Fallback to simple format
        fallback = parse_simple_format(content, {
            "publishers": r"Publishers:\s*(\d+)",
            "subscribers": r"Subscribers:\s*(\d+)",
            "messages_per_pub": r"Messages per publisher:\s*(\d+)",
            "message_size": r"Message size:\s*(\d+)\s*bytes",
            "total_messages": r"Total messages:\s*(\d+)",
            "elapsed_ms": r"Elapsed time:\s*([\d.]+)\s*ms",
            "throughput_msg_per_sec": r"Throughput:\s*([\d.]+)\s*msg/s"
        })
        if fallback:
            result["results"] = [fallback]
        return result

    # Parse columns from header
    columns = [col.strip() for col in header_line.split('|') if col.strip()]

    for line in lines:
        if '|' not in line or 'Transport' in line or '---' in line:
            continue

        parsed = parse_table_line(line, columns)
        if not parsed:
            continue

        # Build entry based on available columns
        entry = {"transport": parsed.get("Transport", "unknown")}

        # Map column names to result keys
        column_mapping = {
            "Message Size": "message_size",
            "Message Count": "message_count",
            "Time": "elapsed_ms",
            "Throughput": "throughput_msg_per_sec",
            "Bandwidth": "bandwidth_mb_per_sec",
            "Subs": "subscribers",
            "Total Msgs": "total_messages"
        }

        for col_name, key in column_mapping.items():
            if col_name in parsed:
                entry[key] = extract_number(parsed[col_name])

        # Only add if we got valid numeric data
        numeric_values = {k: v for k, v in entry.items() if k != "transport"}
        if numeric_values and all(v is not None for v in numeric_values.values()):
            result["results"].append(entry)

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


def load_system_info(input_dir: Path) -> Optional[Dict[str, Any]]:
    """Load system information from sysinfo.json if available."""
    sysinfo_file = input_dir / "sysinfo.json"
    if sysinfo_file.exists():
        try:
            with open(sysinfo_file, 'r', encoding='utf-8') as f:
                return json.load(f)
        except Exception as e:
            print(f"Warning: Failed to load sysinfo.json: {e}", file=sys.stderr)
    return None


def format_as_json(benchmarks: List[Dict[str, Any]], output_file: Path, platform: str,
                   system_info: Optional[Dict[str, Any]] = None):
    """Format benchmarks as JSON."""
    output = {
        "timestamp": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "platform": platform,
        "benchmarks": benchmarks
    }

    if system_info:
        output["system"] = system_info

    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(output, f, indent=2)


def format_as_markdown(benchmarks: List[Dict[str, Any]], output_file: Path, platform: str,
                       system_info: Optional[Dict[str, Any]] = None):
    """Format benchmarks as Markdown table."""
    lines = [
        "# ServerLink Benchmark Results",
        "",
        f"**Date:** {datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M:%S UTC')}  ",
        f"**Platform:** {platform}",
    ]

    # Add system info if available
    if system_info:
        lines.append("")
        lines.append("## System Information")
        lines.append("")
        lines.append("| Property | Value |")
        lines.append("|----------|-------|")
        if "os" in system_info:
            lines.append(f"| OS | {system_info['os']} |")
        if "arch" in system_info:
            lines.append(f"| Architecture | {system_info['arch']} |")
        if "cpu" in system_info:
            lines.append(f"| CPU | {system_info['cpu']} |")
        if "cores" in system_info:
            cores_str = str(system_info['cores'])
            if "logical_processors" in system_info:
                cores_str += f" ({system_info['logical_processors']} threads)"
            lines.append(f"| Cores | {cores_str} |")
        if "memory_gb" in system_info:
            lines.append(f"| Memory | {system_info['memory_gb']} GB |")
        if "kernel" in system_info:
            lines.append(f"| Kernel | {system_info['kernel']} |")
        if "windows_build" in system_info:
            lines.append(f"| Windows Build | {system_info['windows_build']} |")

    lines.extend([
        "",
        "## Summary",
        "",
        "| Benchmark | Type | Transport | Message Size | Key Metric | Value |",
        "|-----------|------|-----------|--------------|------------|-------|"
    ])

    for bench in benchmarks:
        name = bench.get("name", "Unknown")
        bench_type = bench.get("type", "unknown")
        results = bench.get("results", [])

        # Handle new results format
        if results:
            for result in results:
                transport = result.get("transport", "N/A")
                msg_size = result.get("message_size", 0)

                # Determine key metric
                if bench_type == "throughput":
                    key_metric = "Throughput"
                    value = f"{result.get('throughput_msg_per_sec', 0):.0f} msg/s"
                elif bench_type == "latency":
                    key_metric = "Avg Latency"
                    value = f"{result.get('avg_latency_us', 0):.2f} µs"
                elif bench_type == "pubsub":
                    key_metric = "Throughput"
                    value = f"{result.get('throughput_msg_per_sec', 0):.0f} msg/s"
                else:
                    key_metric = "N/A"
                    value = "N/A"

                lines.append(f"| {name} | {bench_type} | {transport} | {msg_size} bytes | {key_metric} | {value} |")
        else:
            # Fallback for old format
            lines.append(f"| {name} | {bench_type} | N/A | N/A | N/A | N/A |")

    lines.extend(["", "## Detailed Results", ""])

    # Add detailed sections for each benchmark
    for bench in benchmarks:
        name = bench.get("name", "Unknown")
        results = bench.get("results", [])

        lines.extend([
            f"### {name}",
            ""
        ])

        if results:
            # Display results as JSON
            lines.append("```json")
            lines.append(json.dumps(results, indent=2))
            lines.append("```")
        else:
            lines.append("No results available")

        lines.append("")

    with open(output_file, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))


def main():
    parser = argparse.ArgumentParser(
        description="ServerLink Benchmark Result Formatter - Parse and format benchmark outputs"
    )
    parser.add_argument(
        "input_dir",
        type=Path,
        help="Directory containing benchmark output files (*.txt)"
    )
    parser.add_argument(
        "output_file",
        type=Path,
        help="Output file path (.json or .md)"
    )
    parser.add_argument(
        "--platform",
        type=str,
        default=sys.platform,
        help="Platform identifier (e.g., linux-x64, win32-x64, darwin-arm64). Default: sys.platform"
    )

    args = parser.parse_args()

    if not args.input_dir.is_dir():
        print(f"Error: Input directory not found: {args.input_dir}", file=sys.stderr)
        sys.exit(1)

    # Find all benchmark result files
    result_files = list(args.input_dir.glob("*.txt"))

    if not result_files:
        print(f"Warning: No .txt files found in {args.input_dir}", file=sys.stderr)
        sys.exit(0)

    # Parse all benchmarks
    benchmarks = []
    for result_file in sorted(result_files):
        parsed = parse_benchmark_file(result_file)
        if parsed:
            benchmarks.append(parsed)
            print(f"Parsed: {result_file.name}")

    if not benchmarks:
        print("Warning: No benchmarks could be parsed", file=sys.stderr)
        sys.exit(0)

    # Load system information if available
    system_info = load_system_info(args.input_dir)
    if system_info:
        print(f"Loaded system info: {system_info.get('os', 'Unknown OS')}, {system_info.get('cpu', 'Unknown CPU')}")

    # Determine output format from extension
    if args.output_file.suffix.lower() == '.json':
        format_as_json(benchmarks, args.output_file, args.platform, system_info)
        print(f"JSON results written to: {args.output_file}")
    elif args.output_file.suffix.lower() == '.md':
        format_as_markdown(benchmarks, args.output_file, args.platform, system_info)
        print(f"Markdown results written to: {args.output_file}")
    else:
        # Default to JSON
        format_as_json(benchmarks, args.output_file, args.platform, system_info)
        print(f"JSON results written to: {args.output_file}")

    print(f"Processed {len(benchmarks)} benchmark(s) for platform: {args.platform}")


if __name__ == "__main__":
    main()
