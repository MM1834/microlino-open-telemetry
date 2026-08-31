#!/usr/bin/env python3
"""Create an exact OPS-001 NotificationRule-only CloudFormation change set."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile


SQL = """SELECT topic() AS mqttTopic, timestamp() AS receivedAt,
encode(*, 'base64') AS payloadBase64 FROM 'mot/+/+/+'
WHERE
(topic(3) = 'charging' AND topic(4) IN ['plugged', 'is_charging', 'power_signed'])
OR (topic(3) = 'display' AND topic(4) IN ['soc', 'odometer_km', 'speed_kmh'])
OR (topic(3) = 'bms' AND topic(4) = 'vehicle_power_w')
OR (topic(3) = 'status' AND topic(4) = 'online')
OR (topic(3) = 'journey' AND topic(4) IN
['energy_counter_id', 'energy_drawn_wh', 'energy_regen_wh', 'energy_net_wh'])"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--region", default="eu-north-1")
    parser.add_argument("--stack-name", default="mot-dev-notifications")
    parser.add_argument("--change-set-name", default="ops-001-notification-filter-minimal-v2-20260824")
    args = parser.parse_args()

    def aws(*command):
        result = subprocess.run(
            ["aws", *command, "--region", args.region, "--output", "json"],
            check=True, capture_output=True, text=True,
        )
        return json.loads(result.stdout)

    deployed = aws(
        "cloudformation", "get-template", "--stack-name", args.stack_name,
        "--template-stage", "Processed",
    )["TemplateBody"]
    if "topic(3) = 'charging'" in deployed:
        print(json.dumps({"ok": True, "changed": False, "reason": "already_deployed"}))
        return
    old_sql = (
        "        Sql: SELECT topic() AS mqttTopic, timestamp() AS receivedAt, encode(*, \n"
        "          'base64') AS payloadBase64 FROM 'mot/+/+/+'"
    )
    new_sql = "        Sql: |-\n" + "\n".join(
        f"          {line}" for line in SQL.splitlines()
    )
    if deployed.count(old_sql) != 1:
        raise SystemExit("deployed NotificationRule SQL marker not found exactly once")
    deployed = deployed.replace(old_sql, new_sql)

    stack = aws(
        "cloudformation", "describe-stacks", "--stack-name", args.stack_name,
    )["Stacks"][0]
    parameter_args = [
        f"ParameterKey={item['ParameterKey']},UsePreviousValue=true"
        for item in stack.get("Parameters", [])
    ]
    with tempfile.TemporaryDirectory(prefix="mot-ops-001-") as temp_dir:
        template_path = Path(temp_dir) / "template.json"
        template_path.write_text(deployed)
        result = aws(
            "cloudformation", "create-change-set",
            "--stack-name", args.stack_name,
            "--change-set-name", args.change_set_name,
            "--change-set-type", "UPDATE",
            "--template-body", f"file://{template_path}",
            "--capabilities", "CAPABILITY_NAMED_IAM",
            "--description", "OPS-001 exact NotificationRule SQL pre-Lambda filter",
            "--parameters", *parameter_args,
        )
    print(json.dumps({
        "ok": True, "changed": True, "changeSetId": result["Id"],
        "changeSetName": args.change_set_name,
    }, separators=(",", ":")))


if __name__ == "__main__":
    main()
