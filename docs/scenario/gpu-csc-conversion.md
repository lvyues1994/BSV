# Scenario: GPU NV12/NV21 To RGBA8888 Conversion
- Given: NV12 or NV21 source buffer and an RGBA8888 destination buffer
- When: GPU conversion is executed via the EGL/OpenGL ES pipeline
- Then: Destination buffer is filled with RGBA8888 pixels

## Test Steps

- Case 1 (happy path): Convert NV12 to RGBA8888 and verify output size and non-zero data.
- Case 2 (happy path): Convert NV21 to RGBA8888 and verify output size and non-zero data.

## Status
- [x] Write scenario document
- [x] Write solid test according to document
- [x] Run test and watch it failing
- [x] Implement to make test pass
- [x] Run test and confirm it passed
- [x] Refactor implementation without breaking test
- [x] Run test and confirm still passing after refactor
