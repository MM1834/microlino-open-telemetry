# AWS-3.3 package

Deploy:

```bash
./tools/aws/deploy_aws_3_3.sh
```

Test:

```bash
./tools/aws/test_aws_3_3_api.sh pioneer
```

Retrieve only the URL:

```bash
./tools/aws/get_aws_3_3_api_url.sh
```

This is a temporary unauthenticated read-only beta API. AWS-4 adds Cognito and
per-user vehicle authorization before end-user deployment.
