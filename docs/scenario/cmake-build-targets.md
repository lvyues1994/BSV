# Scenario: CMake Build Targets For Library And Tests
- Given: Source files for the BSV library and test scenarios
- When: The project is configured and built with CMake
- Then: The static library and test executables are built successfully

## Test Steps

- Case 1 (happy path): Configure and build the project, ensuring the static library target exists.
- Case 2 (happy path): Build the test executables and verify they link to the static library.

## Status
- [x] Write scenario document
- [x] Write solid test according to document
- [x] Run test and watch it failing
- [x] Implement to make test pass
- [x] Run test and confirm it passed
- [x] Refactor implementation without breaking test
- [x] Run test and confirm still passing after refactor
