# Scenario: EGL Context And Shader Compilation
- Given: Linux platform with EGL and OpenGL ES headers available
- When: The converter initializes an EGL context and compiles the shader program
- Then: Initialization succeeds and the shader program is ready for conversion

## Test Steps

- Case 1 (happy path): Create converter, initialize EGL context, and compile shaders successfully.
- Case 2 (edge case): Attempt to initialize with unsupported format and verify it fails.

## Status
- [x] Write scenario document
- [x] Write solid test according to document
- [x] Run test and watch it failing
- [x] Implement to make test pass
- [x] Run test and confirm it passed
- [x] Refactor implementation without breaking test
- [x] Run test and confirm still passing after refactor
