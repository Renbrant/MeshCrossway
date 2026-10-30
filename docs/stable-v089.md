# Owner-confirmed stable v0.8.9

On 2026-10-30 the owner reported that everything works perfectly and requested this revision as the most stable baseline. This supersedes v0.8.2 as the active recommended rollback; historical source remains available. No claim of concurrent BLE support or additional hardware qualification is implied.

Preserve the exact published bytes, not a rebuild:

- Source recorded by the passing PR build: `172f4c2f406d72d3ac3651d55eb5a1b6e3e068e0`.
- Main merge: `a4e10bf635f8fe6c33bdf50cdc05b4351a514dc9`.
- Combined image: 2,036,496 bytes.
- SHA-256: `2adcdebdc91902b2ce0a2e36439dd8162b92ab96e1a3129e9400440e839709d6`.

The build restores this image and verifies its manifest, parts and digest. v0.8.8 is withdrawn for its confirmed boot loop. New versions remain experimental until separately tested on the physical Waveshare.
