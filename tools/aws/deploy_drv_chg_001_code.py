#!/usr/bin/env python3
"""Deploy the two code-only DRV-CHG-001 Lambda updates."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parents[2]


def inline_code(template_path, resource_name):
    lines = template_path.read_text(encoding="utf-8").splitlines()
    start = lines.index(f"  {resource_name}:")
    zip_line = next(
        index for index in range(start, len(lines))
        if lines[index] == "        ZipFile: |"
    )
    code = []
    for line in lines[zip_line + 1:]:
        if line.strip() and not line.startswith("          "):
            break
        code.append(line[10:] if line.strip() else "")
    return "\n".join(code) + "\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--region", default="eu-north-1")
    parser.add_argument("--vehicle-function", default="mot-dev-vehicle-api")
    parser.add_argument("--notification-function", default="mot-dev-notifications")
    args = parser.parse_args()

    def aws(*command):
        result = subprocess.run(
            ["aws", *command, "--region", args.region, "--output", "json"],
            check=True, capture_output=True, text=True,
        )
        return json.loads(result.stdout or "{}")

    with tempfile.TemporaryDirectory(prefix="mot-drv-chg-001-") as directory:
        directory = Path(directory)
        vehicle_zip = directory / "vehicle-api.zip"
        with zipfile.ZipFile(vehicle_zip, "w", zipfile.ZIP_DEFLATED) as package:
            package.writestr(
                "index.py",
                inline_code(
                    ROOT / "cloud/aws/foundation/template.yaml",
                    "VehicleApiFunction",
                ),
            )

        notification_zip = directory / "notifications.zip"
        with zipfile.ZipFile(notification_zip, "w", zipfile.ZIP_DEFLATED) as package:
            for source in sorted((ROOT / "cloud/aws/notifications").glob("*.py")):
                package.write(source, source.name)

        results = []
        for function_name, archive in (
            (args.vehicle_function, vehicle_zip),
            (args.notification_function, notification_zip),
        ):
            updated = aws(
                "lambda", "update-function-code",
                "--function-name", function_name,
                "--zip-file", f"fileb://{archive}",
            )
            subprocess.run(
                ["aws", "lambda", "wait", "function-updated-v2",
                 "--region", args.region, "--function-name", function_name],
                check=True,
            )
            current = aws(
                "lambda", "get-function-configuration",
                "--function-name", function_name,
            )
            results.append({
                "functionName": function_name,
                "version": updated.get("Version"),
                "codeSha256": current.get("CodeSha256"),
                "state": current.get("State"),
                "lastUpdateStatus": current.get("LastUpdateStatus"),
            })

    print(json.dumps({"updates": results}, separators=(",", ":")))


if __name__ == "__main__":
    main()
