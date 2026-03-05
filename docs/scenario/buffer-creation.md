# Scenario: Buffer Creation Paths
- Given: a buffer allocator instance
- When: allocating a buffer internally or importing from an external handle
- Then: a valid buffer instance is returned for both paths

## Test Steps

- Case 1 (happy path): allocate buffer with internal memory and verify metadata
- Case 2 (edge case): import buffer from external handle and verify metadata

## Status
- [x] Write scenario document
- [x] Write solid test according to document
- [x] Run test and watch it failing
- [x] Implement to make test pass
- [x] Run test and confirm it passed
- [x] Refactor implementation without breaking test
- [x] Run test and confirm still passing after refactor
