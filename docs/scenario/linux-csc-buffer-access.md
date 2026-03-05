# Scenario: Linux Buffer Access Control
- Given: a Linux buffer created for CSC
- When: requesting read-only or writable access
- Then: access should be granted appropriately and released cleanly

## Test Steps

- Case 1 (happy path): write to buffer via writable map and read back via read-only map
- Case 2 (edge case): multiple read-only maps are allowed without conflict

## Status
- [x] Write scenario document
- [x] Write solid test according to document
- [x] Run test and watch it failing
- [x] Implement to make test pass
- [x] Run test and confirm it passed
- [x] Refactor implementation without breaking test
- [x] Run test and confirm still passing after refactor
