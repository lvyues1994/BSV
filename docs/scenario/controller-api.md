# Scenario: Controller API thread safety and lifecycle
- Given: A controller instance with valid dependencies
- When: open, close, and selectCamera are called across threads or in invalid order
- Then: Each operation returns explicit error codes and internal state remains consistent

## Test Steps

- Case 1 (happy path): open -> selectCamera -> close returns kOk in sequence
- Case 2 (edge case): open called twice returns kInvalidState or kAlreadyStarted
- Case 3 (edge case): selectCamera while closed returns kInvalidState

## Status
- [x] Write scenario document
- [x] Write solid test according to document
- [x] Run test and watch it failing
- [x] Implement to make test pass
- [x] Run test and confirm it passed
- [x] Refactor implementation without breaking test
- [x] Run test and confirm still passing after refactor
