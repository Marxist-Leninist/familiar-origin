#!/usr/bin/env bash
# Compare the public repository with its private mirror.
# Exit 0 only when main and every tag point at the same objects in both.
set -euo pipefail
pub="${FO_PUBLIC:-$(git remote get-url public 2>/dev/null || true)}"
priv="${FO_PRIVATE:-$(git remote get-url private 2>/dev/null || true)}"
if [ -z "$pub" ] || [ -z "$priv" ]; then
  echo "Set FO_PUBLIC and FO_PRIVATE, or add git remotes named public and private." >&2
  exit 2
fi
refs() { git ls-remote --heads --tags "$1" | grep -v '\^{}$' | awk '{print $2, $1}' | sort; }
a="$(refs "$pub")"
b="$(refs "$priv")"
echo "public  main: $(printf '%s\n' "$a" | awk '$1=="refs/heads/main"{print $2}')"
echo "private main: $(printf '%s\n' "$b" | awk '$1=="refs/heads/main"{print $2}')"
if [ -n "$a" ] && [ "$a" = "$b" ]; then
  echo "IN SYNC ($(printf '%s\n' "$a" | wc -l) refs)"
  exit 0
fi
echo "OUT OF SYNC"
diff <(printf '%s\n' "$a") <(printf '%s\n' "$b") || true
exit 1
