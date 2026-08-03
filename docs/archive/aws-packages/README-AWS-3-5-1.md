# AWS-3.5.1 vehicle-switch fix

Changed:

```text
dashboard/js/app.js
dashboard/js/providers/aws-backend-provider.js
```

After switching from `pioneer` to `beta-01`, all old values must clear
immediately and then be populated only from the selected snapshot.
