# AWS-3.3 CloudFormation indentation fix

The first AWS-3.3 template placed the new API parameters, resources and outputs
at the YAML top level instead of nesting them under:

- `Parameters`
- `Resources`
- `Outputs`

CloudFormation therefore reported them as invalid template properties.

This package contains only the corrected template.
