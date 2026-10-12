#!/usr/bin/env python3
"""Create one atomic factory/update MOT Display WebFlash catalog change set."""

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
    parser.add_argument("--factory-artifact-key", required=True)
    parser.add_argument("--factory-version", required=True)
    parser.add_argument("--factory-sha256", required=True)
    parser.add_argument("--factory-size", required=True, type=int)
    parser.add_argument("--update-artifact-key", required=True)
    parser.add_argument("--update-version", required=True)
    parser.add_argument("--update-sha256", required=True)
    parser.add_argument("--update-size", required=True, type=int)
    args = parser.parse_args()

    stack = aws(
        args.region, "cloudformation", "describe-stacks",
        "--stack-name", args.stack_name,
    )["Stacks"][0]
    replacements = {
        "FirmwareTertiaryArtifactKey": args.factory_artifact_key,
        "FirmwareTertiaryVersion": args.factory_version,
        "FirmwareTertiarySha256": args.factory_sha256,
        "FirmwareTertiarySize": str(args.factory_size),
        "FirmwareQuaternaryArtifactKey": args.update_artifact_key,
        "FirmwareQuaternaryVersion": args.update_version,
        "FirmwareQuaternarySha256": args.update_sha256,
        "FirmwareQuaternarySize": str(args.update_size),
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
        "--description", f"Activate {args.factory_version} for both controlled MOT Display WebFlash paths",
        "--parameters", *parameters,
    )
    print(json.dumps({"changeSetId": result["Id"]}, separators=(",", ":")))


if __name__ == "__main__":
    main()
