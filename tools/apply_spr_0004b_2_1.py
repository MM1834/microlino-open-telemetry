#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path

AUTHORIZER = "VehicleApiJwtAuthorizer"
REQUIRED_RESOURCES = (
    "VehicleHttpApi",
    "DashboardUserPool",
    "DashboardUserPoolClient",
    "VehicleApiIntegration",
)

def find_resource(text: str, logical_id: str):
    return re.search(rf"(?m)^  {re.escape(logical_id)}:\s*$", text)

def resource_block(text: str, logical_id: str) -> str:
    match = find_resource(text, logical_id)
    if not match:
        raise RuntimeError(f"Resource not found: {logical_id}")
    next_match = re.compile(r"(?m)^  [A-Za-z][A-Za-z0-9]+:\s*$").search(text, match.end())
    end = next_match.start() if next_match else len(text)
    return text[match.start():end]

def validate_existing_authorizer(text: str) -> None:
    block = resource_block(text, AUTHORIZER)
    patterns = (
        r"(?m)^    Type:\s*AWS::ApiGatewayV2::Authorizer\s*$",
        r"(?m)^      ApiId:\s*!Ref\s+VehicleHttpApi\s*$",
        r"(?m)^      AuthorizerType:\s*JWT\s*$",
        r'(?m)^        - "\$request\.header\.Authorization"\s*$',
        r"(?m)^          - !Ref\s+DashboardUserPoolClient\s*$",
    )
    for pattern in patterns:
        if not re.search(pattern, block):
            raise RuntimeError(f"{AUTHORIZER} exists but differs from the expected configuration")
    if "cognito-idp.${AWS::Region}.amazonaws.com/${DashboardUserPool}" not in block:
        raise RuntimeError(f"{AUTHORIZER} exists but its Cognito issuer is unexpected")

def ensure_no_route_changes(text: str) -> None:
    for route in ("VehicleApiHealthRoute", "VehicleApiListRoute", "VehicleApiSnapshotRoute"):
        block = resource_block(text, route)
        if re.search(r"(?m)^      AuthorizerId:", block):
            raise RuntimeError(f"{route} already has an AuthorizerId; B.2.1 must not alter routes")
        if re.search(r"(?m)^      AuthorizationType:\s*JWT\s*$", block):
            raise RuntimeError(f"{route} is already JWT-protected; B.2.1 must not alter routes")

def authorizer_yaml() -> str:
    return (
        "  VehicleApiJwtAuthorizer:\n"
        "    Type: AWS::ApiGatewayV2::Authorizer\n"
        "    Properties:\n"
        "      ApiId: !Ref VehicleHttpApi\n"
        "      AuthorizerType: JWT\n"
        "      IdentitySource:\n"
        "        - \"$request.header.Authorization\"\n"
        "      JwtConfiguration:\n"
        "        Audience:\n"
        "          - !Ref DashboardUserPoolClient\n"
        "        Issuer: !Sub \"https://cognito-idp.${AWS::Region}.amazonaws.com/${DashboardUserPool}\"\n"
        "      Name: !Sub \"${ProjectName}-${Environment}-dashboard-jwt\"\n\n"
    )

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--template", default="cloud/aws/foundation/template.yaml")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    path = Path(args.template)
    if not path.is_file():
        print(f"ERROR: template not found: {path}", file=sys.stderr)
        return 2

    original = path.read_text(encoding="utf-8")

    try:
        for resource in REQUIRED_RESOURCES:
            if not find_resource(original, resource):
                raise RuntimeError(f"Resource not found: {resource}")

        if args.check:
            validate_existing_authorizer(original)
            ensure_no_route_changes(original)
            print("OK: JWT authorizer exists and no API route is protected yet")
            return 0

        if find_resource(original, AUTHORIZER):
            validate_existing_authorizer(original)
            ensure_no_route_changes(original)
            print("No changes required; JWT authorizer already exists.")
            return 0

        ensure_no_route_changes(original)

        insertion = find_resource(original, "VehicleApiIntegration")
        if not insertion:
            raise RuntimeError("Insertion anchor not found: VehicleApiIntegration")

        updated = original[:insertion.start()] + authorizer_yaml() + original[insertion.start():]

        validate_existing_authorizer(updated)
        ensure_no_route_changes(updated)

        backup = path.with_suffix(path.suffix + ".spr-0004b2-1.bak")
        shutil.copy2(path, backup)
        path.write_text(updated, encoding="utf-8")

        print(f"Updated: {path}")
        print(f"Backup:  {backup}")
        print("Added:   VehicleApiJwtAuthorizer")
        print("Routes:  unchanged")
        return 0

    except RuntimeError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        print("No speculative changes were made.", file=sys.stderr)
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
