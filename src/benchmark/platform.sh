#!/bin/sh
# Print the platform facts of a benchmark result as "key: value" lines.
# Usage: src/benchmark/platform.sh
# The output has no host name and no user name.
set -eu

if [ $# -ne 0 ]; then
  echo "Usage: $0" >&2
  exit 2
fi

os=$(uname -s)

# Print the first line of the output of a command, or "unknown".
first_line() {
  value=$("$@" 2>/dev/null | head -n 1) || value=
  printf '%s\n' "${value:-unknown}"
}

# Print a value, or "unknown" if it is empty.
or_unknown() {
  printf '%s\n' "${1:-unknown}"
}

# Print a number of bytes in GiB.
gib() {
  awk -v b="$1" 'BEGIN { printf "%.1f GiB\n", b / 1073741824 }'
}

echo "date: $(date -u +%Y-%m-%d)"

if [ "$os" = Darwin ]; then
  system="$(sw_vers -productName 2>/dev/null) $(sw_vers -productVersion 2>/dev/null)"
  cpu=$(sysctl -n machdep.cpu.brand_string 2>/dev/null || true)
  cores=$(sysctl -n hw.physicalcpu 2>/dev/null || true)
  if perf=$(sysctl -n hw.perflevel0.physicalcpu 2>/dev/null) &&
    eff=$(sysctl -n hw.perflevel1.physicalcpu 2>/dev/null); then
    cores="$cores ($perf performance, $eff efficiency)"
  fi
  bytes=$(sysctl -n hw.memsize 2>/dev/null || true)
else
  system=
  if [ -r /etc/os-release ]; then
    system=$(sed -n 's/^PRETTY_NAME=//p' /etc/os-release | head -n 1 | tr -d '"')
  fi
  cpu=
  if command -v lscpu >/dev/null 2>&1; then
    cpu=$(lscpu 2>/dev/null | sed -n 's/^Model name:[[:space:]]*//p' | head -n 1)
  fi
  if [ "$cpu" = - ]; then cpu=; fi
  if [ -z "$cpu" ] && [ -r /proc/cpuinfo ]; then
    cpu=$(sed -n 's/^model name[[:space:]]*:[[:space:]]*//p' /proc/cpuinfo | head -n 1)
  fi
  cores=$(nproc 2>/dev/null || true)
  kb=
  if [ -r /proc/meminfo ]; then
    kb=$(sed -n 's/^MemTotal:[[:space:]]*\([0-9]*\) kB.*/\1/p' /proc/meminfo)
  fi
  bytes=
  if [ -n "$kb" ]; then bytes=$((kb * 1024)); fi
fi

echo "system: $(or_unknown "$(echo "$system" | sed 's/^ *//;s/ *$//')")"
echo "kernel: $(first_line uname -sr)"
echo "machine: $(first_line uname -m)"
echo "cpu: $(or_unknown "$cpu")"
echo "cores: $(or_unknown "$cores")"
if [ -n "${bytes:-}" ]; then
  echo "memory: $(gib "$bytes")"
else
  echo "memory: unknown"
fi
echo "cc: $(first_line "${CC:-cc}" --version)"
echo "c++: $(first_line "${CXX:-c++}" --version)"
if command -v cmake >/dev/null 2>&1; then
  echo "cmake: $(first_line cmake --version)"
else
  echo "cmake: none"
fi
if [ "$os" = Darwin ]; then
  power=$(pmset -g ps 2>/dev/null | head -n 1 || true)
  echo "power: $(or_unknown "$power")"
fi
