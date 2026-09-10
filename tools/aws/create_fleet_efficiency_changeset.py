#!/usr/bin/env python3
"""Create a narrow FLEET-EFF-001 Change Set from the deployed stack template."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--region", default="eu-north-1")
    parser.add_argument("--stack-name", default="mot-dev-notifications")
    parser.add_argument("--change-set-name", default="fleet-eff-001-a-narrow-20260910")
    parser.add_argument("--code-bucket", required=True)
    parser.add_argument("--code-key", required=True)
    args = parser.parse_args()

    def aws(*command):
        result = subprocess.run(
            ["aws", *command, "--region", args.region, "--output", "json"],
            check=True, capture_output=True, text=True,
        )
        return json.loads(result.stdout)

    result = aws(
        "cloudformation", "get-template", "--stack-name", args.stack_name,
        "--template-stage", "Processed",
    )
    template = result["TemplateBody"]
    if isinstance(template, str):
        parsed = subprocess.run(
            [
                "ruby", "-ryaml", "-rjson", "-e",
                "puts JSON.generate(YAML.safe_load(STDIN.read, aliases: true))",
            ],
            input=template, check=True, capture_output=True, text=True,
        )
        template = json.loads(parsed.stdout)
    resources = template["Resources"]
    additions = {
        "VehicleProfileTable", "FleetEfficiencyTable", "FleetJourneyMarkerTable",
        "FleetEfficiencyRole", "FleetEfficiencyFunction",
        "FleetEfficiencyEventSource", "FleetEfficiencyLogGroup",
    }
    present = additions.intersection(resources)
    if present:
        raise SystemExit(f"fleet resources already present: {sorted(present)}")

    resources["EventTable"]["Properties"]["StreamSpecification"] = {
        "StreamViewType": "NEW_IMAGE"
    }

    def table(name, key, component, ttl=False, pitr=False):
        properties = {
            "TableName": {"Fn::Sub": f"${{ProjectName}}-${{Environment}}-{name}"},
            "BillingMode": "PAY_PER_REQUEST",
            "AttributeDefinitions": [{"AttributeName": key, "AttributeType": "S"}],
            "KeySchema": [{"AttributeName": key, "KeyType": "HASH"}],
            "SSESpecification": {"SSEEnabled": True},
            "Tags": [
                {"Key": "project", "Value": {"Ref": "ProjectName"}},
                {"Key": "environment", "Value": {"Ref": "Environment"}},
                {"Key": "component", "Value": component},
            ],
        }
        if ttl:
            properties["TimeToLiveSpecification"] = {
                "AttributeName": "expiresAt", "Enabled": True,
            }
        if pitr:
            properties["PointInTimeRecoverySpecification"] = {
                "PointInTimeRecoveryEnabled": True,
            }
        return {
            "Type": "AWS::DynamoDB::Table",
            "DeletionPolicy": "Retain",
            "UpdateReplacePolicy": "Retain",
            "Properties": properties,
        }

    resources["VehicleProfileTable"] = table(
        "vehicle-profiles", "vehicleId", "fleet-efficiency", pitr=True
    )
    resources["FleetEfficiencyTable"] = table(
        "fleet-efficiency-monthly", "month", "fleet-efficiency", pitr=True
    )
    resources["FleetJourneyMarkerTable"] = table(
        "fleet-efficiency-markers", "markerId", "fleet-efficiency", ttl=True
    )
    resources["FleetEfficiencyRole"] = {
        "Type": "AWS::IAM::Role",
        "Properties": {
            "AssumeRolePolicyDocument": {
                "Version": "2012-10-17",
                "Statement": [{
                    "Effect": "Allow",
                    "Principal": {"Service": ["lambda.amazonaws.com"]},
                    "Action": ["sts:AssumeRole"],
                }],
            },
            "ManagedPolicyArns": [{"Fn::Sub": (
                "arn:${AWS::Partition}:iam::aws:policy/service-role/"
                "AWSLambdaBasicExecutionRole"
            )}],
            "Policies": [{
                "PolicyName": "fleet-efficiency-runtime",
                "PolicyDocument": {
                    "Version": "2012-10-17",
                    "Statement": [
                        {
                            "Effect": "Allow",
                            "Action": [
                                "dynamodb:DescribeStream", "dynamodb:GetRecords",
                                "dynamodb:GetShardIterator", "dynamodb:ListStreams",
                            ],
                            "Resource": {"Fn::GetAtt": ["EventTable", "StreamArn"]},
                        },
                        {
                            "Effect": "Allow", "Action": ["dynamodb:GetItem"],
                            "Resource": {"Fn::GetAtt": ["VehicleProfileTable", "Arn"]},
                        },
                        {
                            "Effect": "Allow",
                            "Action": ["dynamodb:PutItem", "dynamodb:UpdateItem"],
                            "Resource": [
                                {"Fn::GetAtt": ["FleetJourneyMarkerTable", "Arn"]},
                                {"Fn::GetAtt": ["FleetEfficiencyTable", "Arn"]},
                            ],
                        },
                    ],
                },
            }],
        },
    }
    resources["FleetEfficiencyFunction"] = {
        "Type": "AWS::Lambda::Function",
        "Properties": {
            "FunctionName": {"Fn::Sub": "${ProjectName}-${Environment}-fleet-efficiency"},
            "Runtime": "python3.12",
            "Handler": "fleet_efficiency.handler",
            "Timeout": 30,
            "MemorySize": 128,
            "Role": {"Fn::GetAtt": ["FleetEfficiencyRole", "Arn"]},
            "Environment": {"Variables": {
                "VEHICLE_PROFILE_TABLE_NAME": {"Ref": "VehicleProfileTable"},
                "FLEET_MARKER_TABLE_NAME": {"Ref": "FleetJourneyMarkerTable"},
                "FLEET_AGGREGATE_TABLE_NAME": {"Ref": "FleetEfficiencyTable"},
            }},
            "Code": {"S3Bucket": args.code_bucket, "S3Key": args.code_key},
        },
    }
    resources["FleetEfficiencyEventSource"] = {
        "Type": "AWS::Lambda::EventSourceMapping",
        "Properties": {
            "BatchSize": 10,
            "BisectBatchOnFunctionError": True,
            "Enabled": True,
            "EventSourceArn": {"Fn::GetAtt": ["EventTable", "StreamArn"]},
            "FunctionName": {"Ref": "FleetEfficiencyFunction"},
            "MaximumRetryAttempts": 3,
            "StartingPosition": "LATEST",
        },
    }
    resources["FleetEfficiencyLogGroup"] = {
        "Type": "AWS::Logs::LogGroup",
        "Properties": {
            "LogGroupName": {"Fn::Sub": "/aws/lambda/${FleetEfficiencyFunction}"},
            "RetentionInDays": {"Ref": "LogRetentionDays"},
        },
    }
    outputs = template.setdefault("Outputs", {})
    outputs.update({
        "VehicleProfileTableName": {"Value": {"Ref": "VehicleProfileTable"}},
        "FleetEfficiencyTableName": {"Value": {"Ref": "FleetEfficiencyTable"}},
        "FleetJourneyMarkerTableName": {"Value": {"Ref": "FleetJourneyMarkerTable"}},
        "FleetEfficiencyFunctionName": {"Value": {"Ref": "FleetEfficiencyFunction"}},
    })

    stack = aws("cloudformation", "describe-stacks", "--stack-name", args.stack_name)["Stacks"][0]
    parameter_args = [
        f"ParameterKey={item['ParameterKey']},UsePreviousValue=true"
        for item in stack.get("Parameters", [])
    ]
    with tempfile.TemporaryDirectory(prefix="mot-fleet-eff-") as directory:
        path = Path(directory) / "template.json"
        path.write_text(json.dumps(template), encoding="utf-8")
        created = aws(
            "cloudformation", "create-change-set",
            "--stack-name", args.stack_name,
            "--change-set-name", args.change_set_name,
            "--change-set-type", "UPDATE",
            "--template-body", f"file://{path}",
            "--capabilities", "CAPABILITY_NAMED_IAM",
            "--description", "FLEET-EFF-001 narrow additive monthly aggregation",
            "--parameters", *parameter_args,
        )
    print(json.dumps({
        "ok": True,
        "changeSetId": created["Id"],
        "changeSetName": args.change_set_name,
    }, separators=(",", ":")))


if __name__ == "__main__":
    main()
