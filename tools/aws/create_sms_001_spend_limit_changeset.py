#!/usr/bin/env python3
"""Create a live-template Change Set for the SMS USD 10 Lambda gate."""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import tempfile


REGION = "eu-north-1"
STACK = "mot-dev-notifications"
CHANGE_SET = "sms-001-spend-limit-10-20260829"


def aws(*args: str) -> dict:
    result = subprocess.run(
        ["aws", *args, "--output", "json"], check=True,
        capture_output=True, text=True,
    )
    return json.loads(result.stdout or "{}")


template = aws(
    "cloudformation", "get-template", "--region", REGION,
    "--stack-name", STACK, "--template-stage", "Processed",
)["TemplateBody"]
old = "          SMS_EXPECTED_SPEND_LIMIT_USD: '1'"
new = "          SMS_EXPECTED_SPEND_LIMIT_USD: '10'"
if template.count(old) != 1:
    raise RuntimeError("live SMS expected-limit marker is not unique")
template = template.replace(old, new)

stack = aws(
    "cloudformation", "describe-stacks", "--region", REGION,
    "--stack-name", STACK,
)["Stacks"][0]
parameters = [
    {"ParameterKey": entry["ParameterKey"], "UsePreviousValue": True}
    for entry in stack["Parameters"]
]

with tempfile.TemporaryDirectory(prefix="mot-sms-spend-limit-") as directory:
    path = Path(directory) / "template.yaml"
    path.write_text(template, encoding="utf-8")
    result = aws(
        "cloudformation", "create-change-set", "--region", REGION,
        "--stack-name", STACK, "--change-set-name", CHANGE_SET,
        "--change-set-type", "UPDATE", "--template-body", f"file://{path}",
        "--parameters", json.dumps(parameters),
        "--capabilities", "CAPABILITY_NAMED_IAM",
        "--description", "Align Notification Lambda with enforced SMS USD 10 limit",
    )
print(json.dumps(result, indent=2))
