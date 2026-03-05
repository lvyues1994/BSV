# Scenario: Controller pipeline from camera to RGBA output
- Given: Controller opened with camera provider, CSC converter, and RGBA8888 DMA buffer
- When: A camera frame arrives
- Then: The controller converts it via CSC and writes RGBA8888 data into the external buffer

## Test Steps

- Case 1 (happy path): camera callback triggers CSC conversion into output buffer
- Case 2 (edge case): output buffer format mismatch returns kInvalidArgument

## Status
- [x] Write scenario document
- [x] Write solid test according to document
- [x] Run test and watch it failing
- [x] Implement to make test pass
- [x] Run test and confirm it passed
- [x] Refactor implementation without breaking test
- [x] Run test and confirm still passing after refactor
