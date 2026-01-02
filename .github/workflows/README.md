# GitHub Actions Workflows

This directory contains the CI/CD workflows for ServerLink.

## Workflows

### 1. Build (`build.yml`)

**Triggers:**
- Push to `main` or `develop` branches
- Pull requests to `main`
- Tags matching `v*` (for releases)
- Manual dispatch

**What it does:**
- Builds ServerLink for 6 platforms:
  - Linux x64 (epoll)
  - Linux ARM64 (epoll, cross-compiled)
  - Windows x64 (wepoll)
  - Windows ARM64 (wepoll)
  - macOS x64/Intel (kqueue)
  - macOS ARM64/Apple Silicon (kqueue)
- Runs full test suite on native platforms
- Verifies build artifacts
- Creates GitHub releases for version tags

**Build artifacts:**
- Shared libraries (.so, .dll, .dylib)
- Static libraries (.a, .lib)
- Headers (serverlink.h, config.h)
- Test binaries
- Benchmark binaries
- Build info and checksums

### 2. PR Check (`pr-check.yml`)

**Triggers:**
- Pull requests to `main` or `develop`

**What it does:**
- Quick build on Linux x64 (Debug mode)
- Runs all tests with strict warnings (-Werror)
- Code quality checks:
  - File permissions
  - Line endings (no CRLF)
  - TODO/FIXME comments
- Provides fast feedback (typically < 5 minutes)

**Purpose:**
Fast validation for PRs before triggering full multi-platform build.

### 3. Benchmark (`benchmark.yml`)

**Triggers:**
- Pull requests to `main`
- Push to `main`
- Manual dispatch

**What it does:**
- Builds ServerLink in Release mode with optimizations
- Runs performance benchmarks:
  - `bench_throughput` - Message throughput test
  - `bench_latency` - Round-trip latency test
  - `bench_pubsub` - Pub/Sub performance test
- Formats results as JSON and Markdown
- Comments PR with benchmark results
- Compares PR performance vs `main` branch

**Artifacts:**
- `benchmark_results.json` - Machine-readable results
- `benchmark_results.md` - Human-readable results
- `benchmark_comparison.md` - PR vs main comparison

## Platform Support

| Platform | Architecture | Runner | I/O Backend | Tests |
|----------|-------------|--------|-------------|-------|
| Linux | x64 | ubuntu-22.04 | epoll | ✓ |
| Linux | ARM64 | ubuntu-22.04 (cross) | epoll | ✗ |
| Windows | x64 | windows-2022 | wepoll | ✓ |
| Windows | ARM64 | windows-2022 (cross) | wepoll | ✗ |
| macOS | x64 (Intel) | macos-15 | kqueue | ✗ |
| macOS | ARM64 (Apple Silicon) | macos-15 | kqueue | ✓ |

✓ = Native execution, tests run
✗ = Cross-compiled or emulated, tests skipped

## Release Process

To create a new release:

1. Update version in `VERSION` file:
   ```bash
   # Edit VERSION file
   SERVERLINK_VERSION=0.2.0
   ```

2. Commit and push version bump:
   ```bash
   git add VERSION
   git commit -m "chore: Bump version to 0.2.0"
   git push origin main
   ```

3. Create and push tag:
   ```bash
   git tag v0.2.0
   git push origin v0.2.0
   ```

4. GitHub Actions will automatically:
   - Build all 6 platforms
   - Run tests
   - Create GitHub release
   - Upload release assets (zip files + checksums)

## Local Testing

You can run the same builds locally using the build scripts:

### Linux
```bash
./build-scripts/linux/build.sh x64 Release ON
BUILD_DIR=build-x64 ./scripts/run_tests.sh
BUILD_DIR=build-x64 ./scripts/run_benchmarks.sh
```

### Windows
```powershell
.\build-scripts\windows\build.ps1 -Architecture x64 -BuildType Release
.\scripts\run_tests.ps1 -BuildDir build-x64
.\scripts\run_benchmarks.ps1 -BuildDir build-x64
```

### macOS
```bash
./build-scripts/macos/build.sh arm64 Release ON
BUILD_DIR=build-arm64 ./scripts/run_tests.sh
BUILD_DIR=build-arm64 ./scripts/run_benchmarks.sh
```

## Environment Variables

### Build Scripts
- `CLEAN_BUILD=1` - Clean previous build before building
- `BUILD_DIR` - Override default build directory
- `MACOSX_DEPLOYMENT_TARGET` - macOS deployment target (default: 11.0)

### Test Scripts
- `TEST_TIMEOUT` - Test timeout in seconds (default: 300)
- `VERBOSE=1` - Enable verbose test output

### Benchmark Scripts
- `OUTPUT_FILE` - Output file path (default: benchmark_results.json)

## Debugging Workflow Failures

### Check workflow logs
```bash
gh run list --workflow=build.yml
gh run view <run-id>
```

### Download artifacts locally
```bash
gh run download <run-id>
```

### Re-run failed jobs
```bash
gh run rerun <run-id> --failed
```

## Caching

The workflows use GitHub Actions cache for:
- CMake build directories (ccache would be nice but not implemented yet)
- Dependencies (if any external deps are added)

Cache keys are based on:
- Platform and architecture
- Compiler version
- Dependency versions

## Security

- Workflows use pinned action versions (`@v4`, `@v7`)
- Release workflow requires `contents: write` permission
- No secrets are currently used (library is dependency-free)

## Maintenance

### Updating GitHub Actions versions
```bash
# Check for outdated actions
gh actions list

# Update to latest versions in workflow files
# actions/checkout@v4 -> actions/checkout@v5 (when available)
```

### Updating runner versions
- Monitor GitHub's runner deprecation notices
- Update `runs-on` values when new versions are available
- Test on new runners before merging

## Contributing

When modifying workflows:
1. Test changes in a fork first
2. Use `workflow_dispatch` for manual testing
3. Check workflow syntax: `gh workflow view build.yml`
4. Document any new environment variables or features
5. Update this README with changes

## References

- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [ServerLink Build Documentation](../../doc/BUILDING.md)
- [ServerLink Testing Guide](../../TESTING.md)
