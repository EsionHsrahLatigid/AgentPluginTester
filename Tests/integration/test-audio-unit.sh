#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "usage: test-audio-unit.sh <host> <scanner> <fixture.component>" >&2
  exit 2
fi

host="$1"
scanner="$2"
fixture="$3"
components_dir="${HOME}/Library/Audio/Plug-Ins/Components"
installed_fixture="${components_dir}/APH AgentPluginHost Test Gain.component"
work_dir="$(mktemp -d)"
owns_installed_fixture=0

cleanup() {
  if [[ "${owns_installed_fixture}" -eq 1 ]]; then
    rm -rf "${installed_fixture}"
  fi
  rm -rf "${work_dir}"
  killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
}
trap cleanup EXIT

[[ -x "${host}" ]] || { echo "Host is not executable: ${host}" >&2; exit 3; }
[[ -x "${scanner}" ]] || { echo "Scanner is not executable: ${scanner}" >&2; exit 3; }
[[ -d "${fixture}" ]] || { echo "Audio Unit fixture is missing: ${fixture}" >&2; exit 3; }
[[ ! -e "${installed_fixture}" ]] || { echo "Refusing to replace existing fixture: ${installed_fixture}" >&2; exit 3; }

mkdir -p "${components_dir}"
owns_installed_fixture=1
ditto "${fixture}" "${installed_fixture}"
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true

scan_json="${work_dir}/scan.json"
scan_ok=0
for _ in {1..20}; do
  if "${scanner}" "${installed_fixture}" > "${scan_json}"; then
    scan_ok=1
    break
  fi
  sleep 0.25
done

[[ "${scan_ok}" -eq 1 ]] || { cat "${scan_json}" >&2; exit 4; }
jq -e '.passed == true and .plugins[0].format == "AudioUnit"' "${scan_json}" >/dev/null

report="${work_dir}/report.json"
events="${work_dir}/events.ndjson"
"${host}" \
  --mode offline \
  --no-gui \
  --plugin "${installed_fixture}" \
  --source sine \
  --run-seconds 0.1 \
  --report "${report}" \
  --events "${events}"

jq -e '.passed == true
       and .exitCode == 0
       and (.plugins | length) == 1
       and .plugins[0].format == "AudioUnit"
       and .plugins[0].loaded == true' "${report}" >/dev/null
