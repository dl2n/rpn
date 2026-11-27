# Legacy desktop fixtures

These reviewed PNGs were captured from `cf3a8b903afc298fef72a422e47c327fa2714399`,
built as x64 Release with only the registry path redirected. The app used
`SYSTEM_FIXED_FONT` on an ordinary Windows desktop; the Windows build,
resolution, DPI, keyboard layout, and capture executable hash are in
[`profile.json`](profile.json). [`SHA256SUMS`](SHA256SUMS) records the image
hashes. The old runner is preserved at Git tag `test-baseline-legacy-bridge`;
it builds the pinned source rather than relying on any nested checkout.

The two `*-restarted.png` files show historical window placement and are
not expected images for the current app. The other nine images remain
the reference for unchanged rendering.
