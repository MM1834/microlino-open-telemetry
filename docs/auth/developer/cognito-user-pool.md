# Cognito User Pool – developer guide

## Purpose

SPR-0004B.1.1 introduces only the managed user directory. AWS IoT device authentication remains independent and unchanged.

```text
Firmware device -> AWS IoT certificate
Dashboard user  -> Amazon Cognito user pool
```

The user pool authenticates people. It does not assign vehicles or authorize access to telemetry.

## CloudFormation resource

```text
DashboardUserPool (AWS::Cognito::UserPool)
```

The YAML value for MFA must be quoted:

```yaml
MfaConfiguration: "OFF"
```

Unquoted `OFF` can be parsed as Boolean `false`, which Cognito rejects.

## Safe deployment workflow

Validate the template:

```bash
aws cloudformation validate-template \
  --template-body file://cloud/aws/foundation/template.yaml
```

Create a new change set. Do not reuse the failed change-set name:

```bash
aws cloudformation create-change-set \
  --stack-name mot-aws-3-1 \
  --change-set-name spr-0004b1-1-user-pool \
  --change-set-type UPDATE \
  --template-body file://cloud/aws/foundation/template.yaml \
  --parameters \
    ParameterKey=ProjectName,UsePreviousValue=true \
    ParameterKey=Environment,UsePreviousValue=true \
    ParameterKey=TopicFilter,UsePreviousValue=true \
    ParameterKey=LogRetentionDays,UsePreviousValue=true \
    ParameterKey=EnableVerboseMessageLogs,UsePreviousValue=true \
    ParameterKey=ApiAllowedOrigin,UsePreviousValue=true \
    ParameterKey=ApiLogRetentionDays,UsePreviousValue=true \
    ParameterKey=CognitoSelfRegistrationEnabled,ParameterValue=false \
    ParameterKey=CognitoDeletionProtection,ParameterValue=ACTIVE \
  --capabilities CAPABILITY_NAMED_IAM
```

Wait and inspect:

```bash
aws cloudformation wait change-set-create-complete \
  --stack-name mot-aws-3-1 \
  --change-set-name spr-0004b1-1-user-pool

aws cloudformation describe-change-set \
  --stack-name mot-aws-3-1 \
  --change-set-name spr-0004b1-1-user-pool \
  --query 'Changes[].ResourceChange.{Action:Action,LogicalId:LogicalResourceId,Type:ResourceType,Replacement:Replacement}' \
  --output table
```

Expected change:

```text
Add  DashboardUserPool  AWS::Cognito::UserPool  None
```

Execute only after confirming that no existing resource will be replaced:

```bash
aws cloudformation execute-change-set \
  --stack-name mot-aws-3-1 \
  --change-set-name spr-0004b1-1-user-pool

aws cloudformation wait stack-update-complete \
  --stack-name mot-aws-3-1
```

## Verification

```bash
aws cloudformation describe-stacks \
  --stack-name mot-aws-3-1 \
  --query 'Stacks[0].StackStatus' \
  --output text
```

Expected: `UPDATE_COMPLETE`.

```bash
aws cloudformation describe-stacks \
  --stack-name mot-aws-3-1 \
  --query 'Stacks[0].Outputs[?starts_with(OutputKey, `Cognito`)]' \
  --output table
```

The user pool ID and ARN are configuration identifiers, not passwords. Tokens and temporary passwords remain secret.
