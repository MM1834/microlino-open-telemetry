#!/usr/bin/env python3
"""Create the narrow FLEET-EFF-001.G comparison Change Set from live state."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--region", default="eu-north-1")
    parser.add_argument("--stack-name", default="mot-dev-notifications")
    parser.add_argument("--change-set-name", default="fleet-eff-001-g-20260910")
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
    required = {
        "FleetEfficiencyRole", "FleetEfficiencyFunction",
        "FleetEfficiencyTable", "FleetJourneyMarkerTable",
    }
    if not required.issubset(resources):
        raise SystemExit("FLEET-EFF-001 base resources are not deployed")
    additions = {
        "UserEfficiencyTable", "FleetEfficiencyFinalizerRole",
        "FleetEfficiencyFinalizerSchedule", "EfficiencyApiRole",
        "EfficiencyApiFunction", "EfficiencyIntegration", "EfficiencyGetRoute",
        "EfficiencyInvokePermission",
    }
    present = additions.intersection(resources)
    if present:
        raise SystemExit(f"comparison resources already present: {sorted(present)}")

    resources["UserEfficiencyTable"] = {
        "Type": "AWS::DynamoDB::Table",
        "DeletionPolicy": "Retain",
        "UpdateReplacePolicy": "Retain",
        "Properties": {
            "TableName": {"Fn::Sub": "${ProjectName}-${Environment}-user-efficiency-monthly"},
            "BillingMode": "PAY_PER_REQUEST",
            "AttributeDefinitions": [
                {"AttributeName": "month", "AttributeType": "S"},
                {"AttributeName": "subjectVehicleKey", "AttributeType": "S"},
            ],
            "KeySchema": [
                {"AttributeName": "month", "KeyType": "HASH"},
                {"AttributeName": "subjectVehicleKey", "KeyType": "RANGE"},
            ],
            "TimeToLiveSpecification": {"AttributeName": "expiresAt", "Enabled": True},
            "SSESpecification": {"SSEEnabled": True},
            "PointInTimeRecoverySpecification": {"PointInTimeRecoveryEnabled": True},
            "Tags": [
                {"Key": "project", "Value": {"Ref": "ProjectName"}},
                {"Key": "environment", "Value": {"Ref": "Environment"}},
                {"Key": "component", "Value": "personal-efficiency"},
            ],
        },
    }

    policy = resources["FleetEfficiencyRole"]["Properties"]["Policies"][0]["PolicyDocument"]
    write_statement = next(
        item for item in policy["Statement"]
        if "dynamodb:PutItem" in item.get("Action", [])
    )
    for action in ("dynamodb:GetItem", "dynamodb:Query"):
        if action not in write_statement["Action"]:
            write_statement["Action"].append(action)
    write_statement["Resource"].append({"Fn::GetAtt": ["UserEfficiencyTable", "Arn"]})

    fleet_function = resources["FleetEfficiencyFunction"]["Properties"]
    fleet_function["Code"] = {"S3Bucket": args.code_bucket, "S3Key": args.code_key}
    fleet_function["Environment"]["Variables"]["USER_EFFICIENCY_TABLE_NAME"] = {
        "Ref": "UserEfficiencyTable"
    }

    resources["FleetEfficiencyFinalizerRole"] = {
        "Type": "AWS::IAM::Role",
        "Properties": {
            "AssumeRolePolicyDocument": {
                "Version": "2012-10-17",
                "Statement": [{
                    "Effect": "Allow",
                    "Principal": {"Service": ["scheduler.amazonaws.com"]},
                    "Action": ["sts:AssumeRole"],
                }],
            },
            "Policies": [{
                "PolicyName": "invoke-fleet-efficiency-finalizer",
                "PolicyDocument": {
                    "Version": "2012-10-17",
                    "Statement": [{
                        "Effect": "Allow", "Action": ["lambda:InvokeFunction"],
                        "Resource": {"Fn::GetAtt": ["FleetEfficiencyFunction", "Arn"]},
                    }],
                },
            }],
        },
    }
    resources["FleetEfficiencyFinalizerSchedule"] = {
        "Type": "AWS::Scheduler::Schedule",
        "Properties": {
            "Description": "Finalize stored averages for the preceding Zurich calendar month.",
            "ScheduleExpression": "cron(15 0 1 * ? *)",
            "ScheduleExpressionTimezone": "Europe/Zurich",
            "FlexibleTimeWindow": {"Mode": "OFF"},
            "State": "ENABLED",
            "Target": {
                "Arn": {"Fn::GetAtt": ["FleetEfficiencyFunction", "Arn"]},
                "RoleArn": {"Fn::GetAtt": ["FleetEfficiencyFinalizerRole", "Arn"]},
                "Input": '{"type":"finalize_month"}',
            },
        },
    }
    resources["EfficiencyApiRole"] = {
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
                "PolicyName": "personal-community-efficiency-read",
                "PolicyDocument": {
                    "Version": "2012-10-17",
                    "Statement": [{
                        "Effect": "Allow", "Action": ["dynamodb:GetItem"],
                        "Resource": [
                            {"Ref": "AccessTableArn"},
                            {"Fn::GetAtt": ["FleetEfficiencyTable", "Arn"]},
                            {"Fn::GetAtt": ["UserEfficiencyTable", "Arn"]},
                        ],
                    }],
                },
            }],
        },
    }
    resources["EfficiencyApiFunction"] = {
        "Type": "AWS::Lambda::Function",
        "Properties": {
            "FunctionName": {"Fn::Sub": "${ProjectName}-${Environment}-efficiency-comparison"},
            "Runtime": "python3.12", "Handler": "efficiency_api.handler",
            "Timeout": 10, "MemorySize": 128,
            "Role": {"Fn::GetAtt": ["EfficiencyApiRole", "Arn"]},
            "Environment": {"Variables": {
                "ACCESS_TABLE_NAME": {"Ref": "AccessTableName"},
                "FLEET_AGGREGATE_TABLE_NAME": {"Ref": "FleetEfficiencyTable"},
                "USER_EFFICIENCY_TABLE_NAME": {"Ref": "UserEfficiencyTable"},
            }},
            "Code": {"S3Bucket": args.code_bucket, "S3Key": args.code_key},
        },
    }
    resources["EfficiencyIntegration"] = {
        "Type": "AWS::ApiGatewayV2::Integration",
        "Properties": {
            "ApiId": {"Ref": "PreferenceApi"}, "IntegrationType": "AWS_PROXY",
            "IntegrationUri": {"Fn::GetAtt": ["EfficiencyApiFunction", "Arn"]},
            "PayloadFormatVersion": "2.0",
        },
    }
    resources["EfficiencyGetRoute"] = {
        "Type": "AWS::ApiGatewayV2::Route",
        "Properties": {
            "ApiId": {"Ref": "PreferenceApi"},
            "RouteKey": "GET /api/vehicles/{vehicleId}/efficiency-comparison",
            "AuthorizationType": "JWT", "AuthorizerId": {"Ref": "PreferenceAuthorizer"},
            "Target": {"Fn::Sub": "integrations/${EfficiencyIntegration}"},
        },
    }
    resources["EfficiencyInvokePermission"] = {
        "Type": "AWS::Lambda::Permission",
        "Properties": {
            "Action": "lambda:InvokeFunction",
            "FunctionName": {"Ref": "EfficiencyApiFunction"},
            "Principal": "apigateway.amazonaws.com",
            "SourceArn": {"Fn::Sub": (
                "arn:${AWS::Partition}:execute-api:${AWS::Region}:${AWS::AccountId}:"
                "${PreferenceApi}/*/GET/api/vehicles/*/efficiency-comparison"
            )},
        },
    }
    template.setdefault("Outputs", {})["UserEfficiencyTableName"] = {
        "Value": {"Ref": "UserEfficiencyTable"}
    }

    stack = aws("cloudformation", "describe-stacks", "--stack-name", args.stack_name)["Stacks"][0]
    parameter_args = [
        f"ParameterKey={item['ParameterKey']},UsePreviousValue=true"
        for item in stack.get("Parameters", [])
    ]
    with tempfile.TemporaryDirectory(prefix="mot-fleet-eff-g-") as directory:
        path = Path(directory) / "template.json"
        path.write_text(json.dumps(template), encoding="utf-8")
        created = aws(
            "cloudformation", "create-change-set", "--stack-name", args.stack_name,
            "--change-set-name", args.change_set_name, "--change-set-type", "UPDATE",
            "--template-body", f"file://{path}", "--capabilities", "CAPABILITY_NAMED_IAM",
            "--description", "FLEET-EFF-001.G private monthly comparison and stored averages",
            "--parameters", *parameter_args,
        )
    print(json.dumps({"ok": True, "changeSetId": created["Id"]}, separators=(",", ":")))


if __name__ == "__main__":
    main()
