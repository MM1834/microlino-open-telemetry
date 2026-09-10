#!/usr/bin/env python3
"""Create a narrow code-only Change Set for the two fleet-efficiency Lambdas."""

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

    raw = aws(
        "cloudformation", "get-template", "--stack-name", args.stack_name,
        "--template-stage", "Processed",
    )["TemplateBody"]
    if isinstance(raw, str):
        parsed = subprocess.run(
            [
                "ruby", "-ryaml", "-rjson", "-e",
                "puts JSON.generate(YAML.safe_load(STDIN.read, aliases: true))",
            ],
            input=raw, check=True, capture_output=True, text=True,
        )
        template = json.loads(parsed.stdout)
    else:
        template = raw
    code = {"S3Bucket": args.code_bucket, "S3Key": args.code_key}
    for logical_id in ("FleetEfficiencyFunction", "EfficiencyApiFunction"):
        template["Resources"][logical_id]["Properties"]["Code"] = code

    stack = aws("cloudformation", "describe-stacks", "--stack-name", args.stack_name)["Stacks"][0]
    parameters = [
        f"ParameterKey={item['ParameterKey']},UsePreviousValue=true"
        for item in stack.get("Parameters", [])
    ]
    with tempfile.TemporaryDirectory(prefix="mot-fleet-code-") as directory:
        path = Path(directory) / "template.json"
        path.write_text(json.dumps(template), encoding="utf-8")
        created = aws(
            "cloudformation", "create-change-set", "--stack-name", args.stack_name,
            "--change-set-name", args.change_set_name, "--change-set-type", "UPDATE",
            "--template-body", f"file://{path}", "--capabilities", "CAPABILITY_NAMED_IAM",
            "--description", "FLEET-EFF-001.G previous-month boundary correction",
            "--parameters", *parameters,
        )
    print(json.dumps({"ok": True, "changeSetId": created["Id"]}, separators=(",", ":")))


if __name__ == "__main__":
    main()
