# Scenario: Controller error handling and cleanup
- Given: Controller dependencies that return errors
- When: open or frame processing fails
- Then: The controller returns explicit error codes and does not leak resources

## Test Steps

- Case 1 (happy path): close after failed open does not leak or crash
- Case 2 (edge case): CSC failure during frame processing returns kInternal but keeps controller running

## Status
- [x] Write scenario document
- [x] Write solid test according to document
- [x] Run test and watch it failing
- [x] Implement to make test pass
- [x] Run test and confirm it passed
- [x] Refactor implementation without breaking test
- [x] Run test and confirm still passing after refactor
