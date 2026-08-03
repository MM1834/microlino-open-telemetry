#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source_dir="${repo_root}/cloud/aws/onboarding/src"
output_dir="${repo_root}/build/aws/onboarding"
output_file="${output_dir}/onboarding-lambda.zip"

mkdir -p "${output_dir}"
rm -f "${output_file}"
(
  cd "${source_dir}"
  zip -q -X "${output_file}" handler.py
)
printf '%s\n' "${output_file}"
