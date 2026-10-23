#!/usr/bin/env bash
# Fail when the portable bundle cannot load time-tracker.exe or the Qt platform plugin.
set -u

export PATH="$(pwd)/build/bin:${PATH}"

for f in build/bin/*.exe build/bin/*.dll; do
  [[ -e "$f" ]] || continue
  echo "Checking $f"
  ldd "$f" || true
  if ldd "$f" | grep -q "not found"; then
    echo "Missing runtime dependency detected in $f"
    exit 1
  fi
done

if [[ ! -f build/bin/time-tracker.exe ]]; then
  echo "time-tracker.exe was not built"
  exit 1
fi
if [[ ! -f build/bin/platforms/qwindows.dll ]]; then
  echo "Qt platform plugin was not deployed (build/bin/platforms/qwindows.dll)"
  exit 1
fi

if ldd build/bin/platforms/qwindows.dll | grep -q "not found"; then
  echo "Missing runtime dependency detected in platforms/qwindows.dll"
  ldd build/bin/platforms/qwindows.dll || true
  exit 1
fi
