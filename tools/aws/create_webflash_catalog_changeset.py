#!/usr/bin/env python3
"""Create a parameter-only WebFlash catalog change set."""

import argparse
import json
import subprocess


def aws(region, *args):
    result = subprocess.run(
        ["aws", *args, "--region", region, "--output", "json"],
        check=True, capture_output=True, text=True,
    )
    return json.loads(result.stdout or "{}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--region", default="eu-north-1")
    parser.add_argument("--stack-name", default="mot-dev-onboarding")
    parser.add_argument("--change-set-name", required=True)
    parser.add_argument("--artifact-key", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--sha256", required=True)
    parser.add_argument("--size", required=True, type=int)
    args = parser.parse_args()

    stack = aws(
        args.region, "cloudformation", "describe-stacks",
        "--stack-name", args.stack_name,
    )["Stacks"][0]
    replacements = {
        "FirmwareSecondaryArtifactKey": args.artifact_key,
        "FirmwareSecondaryVersion": args.version,
        "FirmwareSecondarySha256": args.sha256,
        "FirmwareSecondarySize": str(args.size),
    }
    parameters = []
    for item in stack.get("Parameters", []):
        key = item["ParameterKey"]
        if key in replacements:
            parameters.append(f"ParameterKey={key},ParameterValue={replacements[key]}")
        else:
            parameters.append(f"ParameterKey={key},UsePreviousValue=true")

    result = aws(
        args.region, "cloudformation", "create-change-set",
        "--stack-name", args.stack_name,
        "--change-set-name", args.change_set_name,
        "--change-set-type", "UPDATE",
        "--use-previous-template",
        "--capabilities", "CAPABILITY_NAMED_IAM",
        "--description", f"Activate {args.version} for controlled N16 WebFlash",
        "--parameters", *parameters,
    )
    print(json.dumps({"changeSetId": result["Id"]}, separators=(",", ":")))


if __name__ == "__main__":
    main()
