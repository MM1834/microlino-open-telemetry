from __future__ import annotations

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
TEMPLATE_PATH = ROOT / "cloud/aws/foundation/template.yaml"


def template_text() -> str:
    return TEMPLATE_PATH.read_text()


def inline_python_blocks(text: str) -> list[str]:
    lines = text.splitlines()
    blocks: list[str] = []
    index = 0
    while index < len(lines):
        line = lines[index]
        if line.lstrip() != "ZipFile: |":
            index += 1
            continue
        base_indent = len(line) - len(line.lstrip())
        code_indent = base_indent + 2
        index += 1
        block: list[str] = []
        while index < len(lines):
            candidate = lines[index]
            indent = len(candidate) - len(candidate.lstrip())
            if candidate.strip() and indent <= base_indent:
                break
            block.append(candidate[code_indent:] if candidate else "")
            index += 1
        blocks.append("\n".join(block))
    return blocks


class InlineLambdaSyntaxTests(unittest.TestCase):
    def test_every_inline_python_block_compiles(self) -> None:
        blocks = inline_python_blocks(template_text())
        self.assertGreaterEqual(len(blocks), 4)
        for number, block in enumerate(blocks, start=1):
            compile(block, f"template-inline-lambda-{number}", "exec")


class AuthorizationInvariantTests(unittest.TestCase):
    def setUp(self) -> None:
        self.template = template_text()

    def test_access_table_uses_user_and_vehicle_key(self) -> None:
        self.assertIn("UserVehicleAccessTable:", self.template)
        self.assertIn('TableName: !Sub "${ProjectName}-${Environment}-user-vehicle-access"', self.template)
        self.assertIn("AttributeName: userSub", self.template)
        self.assertIn("AttributeName: vehicleId", self.template)

    def test_vehicle_api_no_longer_scans_state_table(self) -> None:
        self.assertNotIn("dynamodb:Scan", self.template)
        self.assertNotIn("result = table.scan", self.template)
        self.assertIn('Key("userSub").eq(user_sub)', self.template)

    def test_rest_fails_closed_on_subject_and_assignment(self) -> None:
        self.assertIn('return response(401, {"error": "unauthorized"})', self.template)
        self.assertIn("if not has_active_access(user_sub, vehicle_id):", self.template)
        self.assertIn('"error": "vehicle_not_found"', self.template)

    def test_websocket_expiry_and_assignment_are_enforced(self) -> None:
        self.assertIn('"exp": int(claims["exp"])', self.template)
        self.assertIn('"expiresAt": token_expires_at', self.template)
        self.assertNotIn("int(time.time()) + 86400", self.template)
        self.assertIn("assignment.get(\"status\") != \"ACTIVE\"", self.template)
        self.assertIn("expires_at <= now", self.template)

    def test_access_table_is_available_to_all_enforcement_paths(self) -> None:
        self.assertGreaterEqual(self.template.count("ACCESS_TABLE_NAME"), 4)
        self.assertIn("UserVehicleAccessTableName:", self.template)

    def test_existing_connection_index_projection_is_not_mutated(self) -> None:
        self.assertIn("ProjectionType: KEYS_ONLY", self.template)
        self.assertIn('ProjectionExpression="connectionId"', self.template)
        self.assertIn("connection = connections.get_item(", self.template)

    def test_deployment_origins_have_no_permissive_defaults(self) -> None:
        parameter_section = self.template.split("Conditions:", 1)[0]
        self.assertNotIn('Default: "*"', parameter_section)
        self.assertNotIn("Default: \"http://localhost", parameter_section)

    def test_cognito_managed_login_uses_its_public_domain(self) -> None:
        output_section = self.template.split("Outputs:", 1)[1]
        self.assertGreaterEqual(output_section.count(".amazoncognito.com"), 4)
        self.assertNotIn(
            "DashboardUserPoolDomain}.auth.${AWS::Region}.${AWS::URLSuffix}",
            output_section,
        )


if __name__ == "__main__":
    unittest.main()
