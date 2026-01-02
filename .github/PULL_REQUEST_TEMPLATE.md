## Description

<!-- Provide a brief description of the changes in this PR -->

## Type of Change

<!-- Mark the relevant option with an 'x' -->

- [ ] Bug fix (non-breaking change that fixes an issue)
- [ ] New feature (non-breaking change that adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Performance improvement
- [ ] Code refactoring
- [ ] Documentation update
- [ ] Test coverage improvement
- [ ] Build/CI/CD change

## Related Issues

<!-- Link to related issues using #issue_number -->

Fixes #
Relates to #

## Changes Made

<!-- List the main changes made in this PR -->

-
-
-

## Testing

<!-- Describe the testing you've done -->

### Test Coverage

- [ ] Added new tests for the changes
- [ ] All existing tests pass
- [ ] Manual testing performed

### Platforms Tested

- [ ] Linux x64
- [ ] Linux ARM64
- [ ] Windows x64
- [ ] Windows ARM64
- [ ] macOS x64 (Intel)
- [ ] macOS ARM64 (Apple Silicon)

### Test Commands Run

```bash
# Example:
./build-scripts/linux/build.sh x64 Release ON
BUILD_DIR=build-x64 ./scripts/run_tests.sh
```

## Performance Impact

<!-- If applicable, describe any performance implications -->

- [ ] No performance impact
- [ ] Performance improvement (see benchmarks)
- [ ] Performance regression (justified because...)

### Benchmark Results

<!-- If you ran benchmarks, paste results here or link to workflow artifacts -->

```
# Paste benchmark results or comparison here
```

## Checklist

<!-- Mark completed items with an 'x' -->

### Code Quality

- [ ] Code follows the project's C++20 style guidelines
- [ ] No compiler warnings (tested with `-Wall -Wextra -Wpedantic -Werror`)
- [ ] No undefined behavior (tested with sanitizers if applicable)
- [ ] Memory safety verified (no leaks, no use-after-free)
- [ ] Thread safety considered (if applicable)
- [ ] Error handling is comprehensive

### Documentation

- [ ] Code is self-documenting with clear naming
- [ ] Complex logic has explanatory comments
- [ ] Public API changes are documented
- [ ] README updated (if needed)
- [ ] CLAUDE.md updated (if needed)

### Build System

- [ ] CMake configuration works on all platforms
- [ ] Build scripts updated (if needed)
- [ ] No hardcoded paths or platform-specific assumptions
- [ ] Dependencies are documented

### CI/CD

- [ ] All CI checks pass
- [ ] No test failures
- [ ] Artifacts build successfully
- [ ] Benchmark workflow completes (if applicable)

## Additional Notes

<!-- Any additional information that reviewers should know -->

## Screenshots/Logs

<!-- If applicable, add screenshots or logs to help explain the changes -->

```
# Paste relevant logs here
```

---

**Reviewer Notes:**

- Please check that all tests pass before merging
- Verify that the code adheres to C++20 standards
- Review performance implications if applicable
- Ensure cross-platform compatibility
