# Scenario: CPU Reference Conversion And Comparison
- Given: NV12 or NV21 source buffer and GPU output buffer
- When: CPU reference conversion runs and compares per-pixel values
- Then: GPU output matches CPU reference within allowed tolerance

## Test Steps

- Case 1 (happy path): Compare NV12 GPU output with CPU reference within tolerance.
- Case 2 (happy path): Compare NV21 GPU output with CPU reference within tolerance.

## Status
- [x] Write scenario document
- [x] Write solid test according to document
- [x] Run test and watch it failing
- [x] Implement to make test pass
- [x] Run test and confirm it passed
- [x] Refactor implementation without breaking test
- [x] Run test and confirm still passing after refactor
