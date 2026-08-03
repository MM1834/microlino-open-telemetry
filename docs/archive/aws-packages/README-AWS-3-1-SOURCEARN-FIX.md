# AWS-3.1 SourceArn fix

The original CloudFormation template used a folded multiline YAML scalar:

```yaml
SourceArn: !Sub >
  arn:...
```

This introduced a trailing newline into the Lambda permission `SourceArn`.

The corrected template uses a single-line scalar:

```yaml
SourceArn: !Sub "arn:..."
```

Before redeploying, delete the stack that is in `ROLLBACK_COMPLETE`.
