#!/usr/bin/env python3
"""Create a minimal live-template Change Set for xruser's exact SMS destination."""

from __future__ import annotations

import json
import subprocess
import tempfile
from pathlib import Path


REGION = "eu-north-1"
STACK = "mot-dev-notifications"
CHANGE_SET = "sms-001-xruser-destination-20260826"
DESTINATION_ARN = (
    "arn:aws:sms-voice:eu-north-1:002581114110:"
    "verified-destination-number/vdn-e1d6cd63a74b49a8af94c5dc07eb4f70"
)


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

parameter_anchor = """  ReadOnlyVehicleIds:\n"""
parameter = """  SmsAdditionalVerifiedDestinationNumberArn:\n    Type: String\n    Default: ''\n    AllowedPattern: ^$|^arn:[a-z0-9-]+:sms-voice:[a-z0-9-]+:[0-9]{12}:verified-destination-number/vdn-[a-f0-9]+$\n    Description: Optional second exact verified sandbox destination for the controlled pilot.\n"""
if template.count(parameter_anchor) != 1:
    raise RuntimeError("parameter anchor is not unique")
template = template.replace(parameter_anchor, parameter + parameter_anchor)

condition_anchor = """Conditions:\n  HasSmsAdminPrincipal:\n"""
condition = """Conditions:\n  HasSmsAdditionalVerifiedDestination:\n    Fn::Not:\n    - Fn::Equals:\n      - Ref: SmsAdditionalVerifiedDestinationNumberArn\n      - ''\n  HasSmsAdminPrincipal:\n"""
if template.count(condition_anchor) != 1:
    raise RuntimeError("condition anchor is not unique")
template = template.replace(condition_anchor, condition)

resource_anchor = """            - Ref: SmsVerifiedDestinationNumberArn\n            - Fn::Sub: \n                arn:${AWS::Partition}:sms-voice:${AWS::Region}:${AWS::AccountId}:configuration-set/mot-dev-sms\n"""
resource = """            - Ref: SmsVerifiedDestinationNumberArn\n            - Fn::If:\n              - HasSmsAdditionalVerifiedDestination\n              - Ref: SmsAdditionalVerifiedDestinationNumberArn\n              - Ref: AWS::NoValue\n            - Fn::Sub: \n                arn:${AWS::Partition}:sms-voice:${AWS::Region}:${AWS::AccountId}:configuration-set/mot-dev-sms\n"""
if template.count(resource_anchor) != 1:
    raise RuntimeError("IAM resource anchor is not unique")
template = template.replace(resource_anchor, resource)

stack = aws("cloudformation", "describe-stacks", "--region", REGION, "--stack-name", STACK)["Stacks"][0]
parameters = [
    {"ParameterKey": entry["ParameterKey"], "UsePreviousValue": True}
    for entry in stack["Parameters"]
]
parameters.append({
    "ParameterKey": "SmsAdditionalVerifiedDestinationNumberArn",
    "ParameterValue": DESTINATION_ARN,
})

with tempfile.TemporaryDirectory(prefix="mot-sms-xruser-") as directory:
    path = Path(directory) / "template.yaml"
    path.write_text(template, encoding="utf-8")
    result = aws(
        "cloudformation", "create-change-set", "--region", REGION,
        "--stack-name", STACK, "--change-set-name", CHANGE_SET,
        "--change-set-type", "UPDATE", "--template-body", f"file://{path}",
        "--parameters", json.dumps(parameters), "--capabilities", "CAPABILITY_NAMED_IAM",
        "--description", "Authorize xruser xrpioneer2 exact verified SMS destination",
    )
print(json.dumps(result, indent=2))
