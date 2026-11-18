## Summary

<!-- What changed, and why. -->

## Related issues

<!-- Closes #123 -->

## Type of change

- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change (configuration schema, API or command line)
- [ ] Documentation
- [ ] Build, CI or packaging

## Verification

<!-- The exact commands you ran, and their outcome. -->

```bash
cmake --preset dev
cmake --build --preset dev
ctest --test-dir build/dev --output-on-failure
```

## Checklist

- [ ] Tests cover the new or fixed behaviour, or the change is documentation-only
- [ ] `docs/` updated when a command, endpoint, option or profile field changed
- [ ] `CHANGELOG.md` updated under `[Unreleased]` for user-visible changes
- [ ] No tokens, keys, private host names or local paths added
- [ ] Files touched by the change were formatted with `clang-format`
