# Keeping the two repositories in step

Familiar Origin is published to two GitHub repositories:

- `Marxist-Leninist/familiar-origin` (public): the source of truth.
- a private mirror, which must always hold the same commits and tags.

Rules:

1. Every release is one commit on `main`, tagged `vX.Y.Z`, with `VERSION` holding the same number.
2. `tools/publish_both.sh` pushes that commit and its tag to both repositories and fails unless both end on the same commit.
3. `tools/check_sync.sh` compares the two repositories at any time. It prints both heads and exits non-zero if the branch or any tag differs.
4. Nothing is pushed to the private mirror on its own. If the two ever differ, the public repository wins: fast-forward the mirror from it.

Both scripts read the two remote URLs from the `FO_PUBLIC` and `FO_PRIVATE` environment variables, or from git remotes named `public` and `private`.

## Website

https://zqx.lat/familiar-origin/ serves `browser-site/dist` from the tagged commit. The stylesheet and script links in `index.html`, `hello.html` and `licenses.html` carry `?v=<version>`. Change them together with `VERSION`, so a browser or CDN cache can never pair a new page with an old script.

## Release bundle

`MANIFEST.sha256` describes the release bundle (the zip attached to each GitHub release). The bundle includes the prebuilt `build/` directory, which git ignores. In a plain checkout the four `build/` entries are therefore missing until you run `make`, and a different compiler version produces different bytes for the kernel and the disk image. The image the website boots is `browser-site/dist/assets/origin.img`, which is committed.
