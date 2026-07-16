# AWS-3.2 validation checklist

- [ ] CloudFormation reaches `UPDATE_COMPLETE`
- [ ] DynamoDB table exists
- [ ] `status/online` item appears
- [ ] `system/last_seen_utc` changes over time
- [ ] heartbeat is stored as a DynamoDB map
- [ ] system/location values appear without CAN
- [ ] display/charging values appear with CAN
- [ ] second vehicle ID creates a separate partition
- [ ] per-message verbose logs are disabled
