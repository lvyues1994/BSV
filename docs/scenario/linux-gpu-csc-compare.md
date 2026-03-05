# Scenario: Linux GPU CSC Output Matches CPU Reference
- Given: NV12 or NV21 source buffers with deterministic patterns
- When: The GPU CSC converts to RGBA8888 and CPU reference conversion runs
- Then: GPU output matches CPU reference per pixel within tolerance

## Test Steps

- Case 1 (happy path): Convert NV12 to RGBA8888 and compare GPU vs CPU outputs.
- Case 2 (happy path): Convert NV21 to RGBA8888 and compare GPU vs CPU outputs.

## Status
- [x] Write scenario document
- [x] Write solid test according to document
- [x] Run test and watch it failing
- [x] Implement to make test pass
- [x] Run test and confirm it passed
- [x] Refactor implementation without breaking test
- [x] Run test and confirm still passing after refactor
