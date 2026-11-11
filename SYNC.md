# Keeping the two repositories in step

Familiar Origin is published to two GitHub repositories:

- `Marxist-Leninist/familiar-origin` (public): the source of truth.
- a private mirror, which must always hold the same commits and tags.

Rules:

1. Every release is one commit on `main`, tagged `vX.Y.Z`, with `VERSION` holding the same number.
2. `tools/publish_both.sh` pushes that commit and its tag to both repositories and fails unless both end on the same commit.
3. `tools/check_sync.sh` compares the two repositories at any time. It prints both heads and exits non-zero if the branch or any tag differs.
4. Nothing is pushed to the private mirror on its own. If the two ever differ, the public repository wins: fast-forward the mirror from it.
5. Corrections to documents and tests may be committed to `main` between releases without a tag. They are pushed to both repositories in the same step. The website and the release bundle still come from the latest tagged commit, so nothing under `browser-site/dist` or `src` and neither disk image changes outside a release. `MANIFEST.sha256` is updated for the corrected files, so it keeps matching the committed tree; the bundle attached to the last release keeps its own copy.

Both scripts read the two remote URLs from the `FO_PUBLIC` and `FO_PRIVATE` environment variables, or from git remotes named `public` and `private`.

## Website

https://zqx.lat/familiar-origin/ serves `browser-site/dist` from the tagged commit. The stylesheet and script links in `index.html`, `hello.html` and `licenses.html` carry `?v=<version>`, and so does the disk image address in `app.js`. Change them together with `VERSION`, so a browser or CDN cache can never pair a new page with an old script or an old disk image. `tests/test_browser.mjs` fails if the page or the booted kernel reports a different version from `VERSION`.

## Release bundle

`MANIFEST.sha256` describes the release bundle (the zip attached to each GitHub release). The bundle includes the prebuilt `build64/` and `build32/` directories, which git ignores. In a plain checkout those entries are therefore missing until you run `make` and `make ARCH=32`, and a different compiler version produces different bytes for the kernels and the disk images. The image the website boots is `browser-site/dist/assets/origin.img` (32-bit); the native 64-bit image is `browser-site/dist/assets/origin64.img`. Both are committed.
