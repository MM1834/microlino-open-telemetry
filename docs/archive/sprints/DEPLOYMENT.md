# Deployment — SPR-0004B.2.1

```bash
cd /Users/martin/Documents/MICROLINO/microlino-open-telemetry
```

Paketinhalt in das Repository kopieren und dann:

```bash
python3 tools/apply_spr_0004b_2_1.py
python3 tools/apply_spr_0004b_2_1.py --check

git diff -- cloud/aws/foundation/template.yaml
git diff --check
```

Danach validieren:

```bash
aws cloudformation validate-template   --template-body file://cloud/aws/foundation/template.yaml
```

Change Set:

```bash
aws cloudformation create-change-set   --stack-name mot-aws-3-1   --change-set-name spr-0004b2-1-jwt-authorizer   --change-set-type UPDATE   --template-body file://cloud/aws/foundation/template.yaml   --parameters     ParameterKey=ProjectName,UsePreviousValue=true     ParameterKey=Environment,UsePreviousValue=true     ParameterKey=TopicFilter,UsePreviousValue=true     ParameterKey=LogRetentionDays,UsePreviousValue=true     ParameterKey=EnableVerboseMessageLogs,UsePreviousValue=true     ParameterKey=ApiAllowedOrigin,UsePreviousValue=true     ParameterKey=ApiLogRetentionDays,UsePreviousValue=true     ParameterKey=CognitoSelfRegistrationEnabled,UsePreviousValue=true     ParameterKey=CognitoDeletionProtection,UsePreviousValue=true     ParameterKey=CognitoDashboardCallbackUrls,UsePreviousValue=true     ParameterKey=CognitoDashboardLogoutUrls,UsePreviousValue=true   --capabilities CAPABILITY_NAMED_IAM
```

```bash
aws cloudformation wait change-set-create-complete   --stack-name mot-aws-3-1   --change-set-name spr-0004b2-1-jwt-authorizer
```

Prüfen:

```bash
aws cloudformation describe-change-set   --stack-name mot-aws-3-1   --change-set-name spr-0004b2-1-jwt-authorizer   --query 'Changes[].ResourceChange.{Action:Action,LogicalId:LogicalResourceId,Type:ResourceType,Replacement:Replacement}'   --output table
```

Erwartet ausschließlich:

```text
Add | VehicleApiJwtAuthorizer | AWS::ApiGatewayV2::Authorizer | None
```

Dann ausführen:

```bash
aws cloudformation execute-change-set   --stack-name mot-aws-3-1   --change-set-name spr-0004b2-1-jwt-authorizer

aws cloudformation wait stack-update-complete   --stack-name mot-aws-3-1
```

Nach erfolgreichem B.2.1-Deployment:

```bash
python3 tools/apply_spr_0004b_2_2.py
python3 tools/apply_spr_0004b_2_2.py --check
```
