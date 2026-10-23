#!/usr/bin/env bash
# Copy MinGW and Qt DLLs from the UCRT64 prefix next to the exe.
# Repeats so a newly copied DLL can pull its own dependencies.
set -u

export PATH="$(pwd)/build/bin:${PATH}"

for _pass in 1 2 3; do
  echo "Collecting runtime DLLs, pass ${_pass}"
  rm -f deps.txt
  while IFS= read -r f; do
    ldd "$f" | awk '/\/ucrt64\/bin\// { print $3 }' >> deps.txt || true
  done < <(find build/bin -type f \( -name '*.exe' -o -name '*.dll' \))
  if [[ -s deps.txt ]]; then
    sort -u deps.txt | while read -r dep; do
      if [[ -f "$dep" ]]; then
        cp -n "$dep" build/bin/ || true
      fi
    done
  fi
done
