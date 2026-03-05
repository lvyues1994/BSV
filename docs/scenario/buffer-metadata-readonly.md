# Scenario: Buffer Metadata Read-Only Access
- Given: a buffer instance
- When: querying its descriptor
- Then: width, height, stride, and format are available as read-only values

## Test Steps

- Case 1 (happy path): verify descriptor values match allocation request
- Case 2 (edge case): descriptor remains consistent after mapping/unmapping

## Status
- [x] Write scenario document
- [x] Write solid test according to document
- [x] Run test and watch it failing
- [x] Implement to make test pass
- [x] Run test and confirm it passed
- [x] Refactor implementation without breaking test
- [x] Run test and confirm still passing after refactor
