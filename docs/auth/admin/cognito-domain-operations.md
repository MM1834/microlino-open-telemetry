# Cognito domain operations

## Deployment verification

After deployment, confirm that the stack is `UPDATE_COMPLETE` and that `DashboardUserPoolDomain` is `CREATE_COMPLETE`.

```bash
aws cloudformation list-stack-resources \
  --stack-name mot-aws-3-1 \
  --query 'StackResourceSummaries[?LogicalResourceId==`DashboardUserPoolDomain`].{LogicalId:LogicalResourceId,PhysicalId:PhysicalResourceId,Status:ResourceStatus}' \
  --output table
```

Read the generated prefix and base URL from the stack outputs:

```bash
aws cloudformation describe-stacks \
  --stack-name mot-aws-3-1 \
  --query 'Stacks[0].Outputs[?starts_with(OutputKey, `Cognito`)]' \
  --output table
```

Then inspect the domain directly:

```bash
DOMAIN_PREFIX=$(aws cloudformation describe-stacks \
  --stack-name mot-aws-3-1 \
  --query 'Stacks[0].Outputs[?OutputKey==`CognitoDomainPrefix`].OutputValue | [0]' \
  --output text)

aws cognito-idp describe-user-pool-domain \
  --domain "$DOMAIN_PREFIX"
```

## Operational notes

- The prefix is part of the public login URL but is not a secret.
- DNS and service propagation can take a short time after creation.
- Do not delete or rename the domain casually; dashboard login configuration will depend on its URL.
- A custom branded domain is outside this increment.
