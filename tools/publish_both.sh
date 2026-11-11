#!/usr/bin/env bash
# Push the current commit on main and the tag for VERSION to the public
# repository and its private mirror, then confirm both ended on the same commit.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"
version="$(tr -d ' \r\n' < VERSION)"
tag="v$version"
[ "$(git rev-parse --abbrev-ref HEAD)" = "main" ] || { echo "Not on main." >&2; exit 2; }
[ -z "$(git status --porcelain)" ] || { echo "Working tree is not clean." >&2; exit 2; }
git rev-parse -q --verify "refs/tags/$tag" >/dev/null || { echo "Tag $tag does not exist. Create it first." >&2; exit 2; }
[ "$(git rev-parse "$tag^{commit}")" = "$(git rev-parse HEAD)" ] || { echo "Tag $tag is not at HEAD." >&2; exit 2; }
for remote in public private; do
  git remote get-url "$remote" >/dev/null 2>&1 || { echo "Missing git remote: $remote" >&2; exit 2; }
done
git push public main "$tag"
git push private main "$tag"
exec tools/check_sync.sh
