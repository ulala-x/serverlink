#!/usr/bin/env python3
"""
ServerLink Benchmark Comparison Tool

Reads multiple platform benchmark JSON results and generates comparison reports
in both Markdown and JSON formats.

Usage:
    python compare_benchmarks.py --input-dir benchmarks/ --output-md report.md --output-json report.json
"""

import argparse
import json
import sys
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Any, Optional
from collections import defaultdict


class BenchmarkComparator:
    """Compares benchmark results across multiple platforms."""

    def __init__(self, input_dir: Path, version: str = "v0.2.0"):
        self.input_dir = input_dir
        self.version = version
        self.platforms: List[str] = []
        self.benchmark_data: Dict[str, Any] = {}
        self.system_info: Dict[str, Dict[str, Any]] = {}
        self.throughput_data: Dict[str, Dict[str, Optional[float]]] = defaultdict(dict)
        self.latency_data: Dict[str, Dict[str, Dict[str, Optional[float]]]] = defaultdict(lambda: defaultdict(dict))
        self.bandwidth_data: Dict[str, Dict[str, Optional[float]]] = defaultdict(dict)

    def load_benchmarks(self) -> bool:
        """Load all benchmark JSON files from input directory."""
        if not self.input_dir.exists():
            print(f"Error: Input directory '{self.input_dir}' does not exist", file=sys.stderr)
            return False

        json_files = list(self.input_dir.glob("benchmark-*.json"))
        if not json_files:
            print(f"Error: No benchmark-*.json files found in '{self.input_dir}'", file=sys.stderr)
            return False

        print(f"Found {len(json_files)} benchmark file(s)")

        for json_file in sorted(json_files):
            try:
                with open(json_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)

                platform = data.get('platform', 'unknown')
                self.platforms.append(platform)
                self.benchmark_data[platform] = data

                # Extract system info if available
                if 'system' in data:
                    self.system_info[platform] = data['system']

                print(f"  Loaded: {json_file.name} (platform: {platform})")
                self._process_platform_data(platform, data)

            except json.JSONDecodeError as e:
                print(f"Warning: Failed to parse {json_file.name}: {e}", file=sys.stderr)
                continue
            except Exception as e:
                print(f"Warning: Failed to load {json_file.name}: {e}", file=sys.stderr)
                continue

        if not self.platforms:
            print("Error: No valid benchmark data loaded", file=sys.stderr)
            return False

        return True

    def _process_platform_data(self, platform: str, data: Dict[str, Any]) -> None:
        """Process benchmark data for a single platform."""
        benchmarks = data.get('benchmarks', [])

        for bench in benchmarks:
            bench_type = bench.get('type', '')
            results = bench.get('results', [])

            for result in results:
                transport = result.get('transport', 'unknown')
                msg_size = result.get('message_size', 0)
                key = f"{transport}_{msg_size}B"

                if bench_type == 'throughput':
                    throughput = result.get('throughput_msg_per_sec')
                    bandwidth = result.get('bandwidth_mb_per_sec')

                    if throughput is not None:
                        self.throughput_data[key][platform] = throughput
                    if bandwidth is not None:
                        self.bandwidth_data[key][platform] = bandwidth

                elif bench_type == 'latency':
                    avg = result.get('avg_us')
                    p50 = result.get('p50_us')
                    p95 = result.get('p95_us')
                    p99 = result.get('p99_us')

                    if avg is not None:
                        self.latency_data[key][platform]['avg'] = avg
                    if p50 is not None:
                        self.latency_data[key][platform]['p50'] = p50
                    if p95 is not None:
                        self.latency_data[key][platform]['p95'] = p95
                    if p99 is not None:
                        self.latency_data[key][platform]['p99'] = p99

    def _format_number(self, value: Optional[float], decimals: int = 0) -> str:
        """Format number with thousand separators."""
        if value is None:
            return "N/A"

        if decimals == 0:
            return f"{int(value):,}"
        else:
            return f"{value:,.{decimals}f}"

    def _format_platform_name(self, platform: str) -> str:
        """Convert platform identifier to human-readable name."""
        platform_names = {
            'linux-x64': 'Linux x64',
            'linux-arm64': 'Linux ARM64',
            'windows-x64': 'Windows x64',
            'windows-arm64': 'Windows ARM64',
            'macos-x64': 'macOS x64',
            'macos-arm64': 'macOS ARM64',
        }
        return platform_names.get(platform, platform)

    def _parse_key(self, key: str) -> tuple:
        """Parse benchmark key into transport and message size."""
        parts = key.rsplit('_', 1)
        if len(parts) != 2:
            return (key, "")

        transport = parts[0]
        msg_size = parts[1]
        return (transport, msg_size)

    def generate_markdown(self, output_path: Path) -> bool:
        """Generate Markdown comparison report."""
        try:
            with open(output_path, 'w', encoding='utf-8') as f:
                # Header
                f.write(f"# ServerLink Performance Benchmarks\n\n")
                f.write(f"**Version:** {self.version}\n")
                f.write(f"**Generated:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")

                # Platform list with system info
                f.write("## Tested Platforms\n\n")
                if self.system_info:
                    f.write("| Platform | OS | CPU | Cores | Memory |\n")
                    f.write("|----------|-----|-----|-------|--------|\n")
                    for platform in sorted(self.platforms):
                        info = self.system_info.get(platform, {})
                        os_name = info.get('os', 'N/A')
                        cpu = info.get('cpu', 'N/A')
                        # Truncate long CPU names
                        if len(cpu) > 40:
                            cpu = cpu[:37] + "..."
                        cores = info.get('cores', 'N/A')
                        if 'logical_processors' in info:
                            cores = f"{cores} ({info['logical_processors']}T)"
                        memory = f"{info.get('memory_gb', 'N/A')} GB" if 'memory_gb' in info else 'N/A'
                        f.write(f"| {self._format_platform_name(platform)} | {os_name} | {cpu} | {cores} | {memory} |\n")
                else:
                    for platform in sorted(self.platforms):
                        f.write(f"- {self._format_platform_name(platform)}\n")
                f.write("\n")

                # Throughput section
                if self.throughput_data:
                    f.write("## Throughput (msg/s)\n\n")
                    self._write_table(f, self.throughput_data, decimals=0)
                    f.write("\n")

                # Latency sections
                if self.latency_data:
                    for metric in ['p50', 'p95', 'p99', 'avg']:
                        metric_data = {}
                        for key, platforms in self.latency_data.items():
                            metric_data[key] = {
                                platform: data.get(metric)
                                for platform, data in platforms.items()
                            }

                        if any(metric_data.values()):
                            metric_name = {
                                'avg': 'Average',
                                'p50': 'Median (p50)',
                                'p95': '95th Percentile',
                                'p99': '99th Percentile'
                            }[metric]

                            f.write(f"## Latency {metric_name} (μs)\n\n")
                            self._write_table(f, metric_data, decimals=2)
                            f.write("\n")

                # Bandwidth section
                if self.bandwidth_data:
                    f.write("## Bandwidth (MB/s)\n\n")
                    self._write_table(f, self.bandwidth_data, decimals=2)
                    f.write("\n")

                # Footer
                f.write("---\n\n")
                f.write("*Generated by ServerLink benchmark comparison tool*\n")

            print(f"Generated Markdown report: {output_path}")
            return True

        except Exception as e:
            print(f"Error: Failed to generate Markdown report: {e}", file=sys.stderr)
            return False

    def _write_table(self, f, data: Dict[str, Dict[str, Optional[float]]], decimals: int) -> None:
        """Write a comparison table to Markdown file."""
        if not data:
            return

        # Sort platforms for consistent column order
        sorted_platforms = sorted(self.platforms)

        # Table header
        platform_headers = [self._format_platform_name(p) for p in sorted_platforms]
        f.write("| Transport | Message | " + " | ".join(platform_headers) + " |\n")
        f.write("|-----------|---------|" + "|".join(["-" * (len(h) + 2) for h in platform_headers]) + "|\n")

        # Table rows
        for key in sorted(data.keys()):
            transport, msg_size = self._parse_key(key)

            row = [transport, msg_size]
            for platform in sorted_platforms:
                value = data[key].get(platform)
                row.append(self._format_number(value, decimals))

            f.write("| " + " | ".join(row) + " |\n")

    def generate_json(self, output_path: Path) -> bool:
        """Generate JSON comparison report."""
        try:
            report = {
                "version": self.version,
                "generated_at": datetime.now().isoformat(),
                "platforms": sorted(self.platforms),
                "system_info": self.system_info,
                "throughput": self._convert_to_json_format(self.throughput_data),
                "latency": {
                    "avg": self._convert_latency_to_json('avg'),
                    "p50": self._convert_latency_to_json('p50'),
                    "p95": self._convert_latency_to_json('p95'),
                    "p99": self._convert_latency_to_json('p99'),
                },
                "bandwidth": self._convert_to_json_format(self.bandwidth_data),
            }

            with open(output_path, 'w', encoding='utf-8') as f:
                json.dump(report, f, indent=2)

            print(f"Generated JSON report: {output_path}")
            return True

        except Exception as e:
            print(f"Error: Failed to generate JSON report: {e}", file=sys.stderr)
            return False

    def _convert_to_json_format(self, data: Dict[str, Dict[str, Optional[float]]]) -> Dict:
        """Convert internal data format to JSON output format."""
        result = {}
        for key, platforms in data.items():
            result[key] = {
                platform: value
                for platform, value in platforms.items()
                if value is not None
            }
        return result

    def _convert_latency_to_json(self, metric: str) -> Dict:
        """Convert latency data for a specific metric to JSON format."""
        result = {}
        for key, platforms in self.latency_data.items():
            result[key] = {
                platform: data.get(metric)
                for platform, data in platforms.items()
                if data.get(metric) is not None
            }
        return result


def main():
    parser = argparse.ArgumentParser(
        description="Compare ServerLink benchmarks across multiple platforms",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Compare all benchmarks in benchmarks/ directory
  python compare_benchmarks.py --input-dir benchmarks/

  # Generate custom output files
  python compare_benchmarks.py --input-dir benchmarks/ \\
    --output-md report.md --output-json report.json

  # Specify version
  python compare_benchmarks.py --input-dir benchmarks/ --version v0.3.0
        """
    )

    parser.add_argument(
        '--input-dir',
        type=Path,
        required=True,
        help='Directory containing benchmark-*.json files'
    )
    parser.add_argument(
        '--output-md',
        type=Path,
        default=Path('benchmark_report.md'),
        help='Output Markdown file (default: benchmark_report.md)'
    )
    parser.add_argument(
        '--output-json',
        type=Path,
        default=Path('benchmark_report.json'),
        help='Output JSON file (default: benchmark_report.json)'
    )
    parser.add_argument(
        '--version',
        type=str,
        default='v0.2.0',
        help='ServerLink version (default: v0.2.0)'
    )

    args = parser.parse_args()

    # Create comparator and load benchmarks
    comparator = BenchmarkComparator(args.input_dir, args.version)

    print(f"Loading benchmarks from: {args.input_dir}")
    if not comparator.load_benchmarks():
        return 1

    # Generate reports
    print("\nGenerating comparison reports...")

    md_success = comparator.generate_markdown(args.output_md)
    json_success = comparator.generate_json(args.output_json)

    if md_success and json_success:
        print("\n✅ Benchmark comparison completed successfully!")
        return 0
    else:
        print("\n⚠️  Benchmark comparison completed with errors", file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
