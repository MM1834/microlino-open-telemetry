# AWS-3.3 validation checklist

> **Status:** Historical validation record; not evidence for the current commit or AWS account.

- [ ] CloudFormation reaches `UPDATE_COMPLETE`
- [ ] `VehicleApiBaseUrl` appears in stack outputs
- [ ] `GET /health` returns `ok: true`
- [ ] `GET /api/vehicles` lists `pioneer`
- [ ] snapshot returns current status/system values
- [ ] unknown vehicle returns HTTP 404
- [ ] API Lambda has read-only DynamoDB permissions
- [ ] CORS works from the intended Dashboard origin
- [ ] no device certificate is exposed to the browser
- [ ] public beta API limitation is documented
