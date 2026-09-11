#!/usr/bin/env python3
"""Create a narrow code-only NTF-I18N-001 CloudFormation Change Set."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--region", default="eu-north-1")
    parser.add_argument("--stack-name", default="mot-dev-notifications")
    parser.add_argument("--change-set-name", required=True)
    parser.add_argument("--code-bucket", required=True)
    parser.add_argument("--code-key", required=True)
    args = parser.parse_args()

    def aws(*command):
        result = subprocess.run(
            ["aws", *command, "--region", args.region, "--output", "json"],
            check=True, capture_output=True, text=True,
        )
        return json.loads(result.stdout)

    template = aws(
        "cloudformation", "get-template", "--stack-name", args.stack_name,
        "--template-stage", "Processed",
    )["TemplateBody"]
    if isinstance(template, str):
        raise SystemExit("Expected processed template object")
    code = {"S3Bucket": args.code_bucket, "S3Key": args.code_key}
    for logical_id in ("NotificationFunction", "PreferenceApiFunction"):
        template["Resources"][logical_id]["Properties"]["Code"] = code

    stack = aws(
        "cloudformation", "describe-stacks", "--stack-name", args.stack_name
    )["Stacks"][0]
    parameters = [
        f"ParameterKey={item['ParameterKey']},UsePreviousValue=true"
        for item in stack.get("Parameters", [])
    ]
    with tempfile.TemporaryDirectory(prefix="mot-ntf-i18n-") as directory:
        path = Path(directory) / "template.json"
        path.write_text(json.dumps(template), encoding="utf-8")
        created = aws(
            "cloudformation", "create-change-set",
            "--stack-name", args.stack_name,
            "--change-set-name", args.change_set_name,
            "--change-set-type", "UPDATE",
            "--template-body", f"file://{path}",
            "--capabilities", "CAPABILITY_NAMED_IAM",
            "--description", "NTF-I18N-001 localized email and SMS templates",
            "--parameters", *parameters,
        )
    print(json.dumps({"changeSetId": created["Id"]}, separators=(",", ":")))


if __name__ == "__main__":
    main()
