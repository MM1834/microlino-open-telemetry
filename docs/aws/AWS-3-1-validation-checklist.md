# AWS-3.1 validation checklist

- [ ] CloudFormation reaches `CREATE_COMPLETE`
- [ ] IoT Rule is enabled
- [ ] `status/online` appears in Lambda logs
- [ ] `system/heartbeat` is classified as `json_object`
- [ ] `system/last_seen_utc` is classified as `number`
- [ ] location topics appear without CAN
- [ ] display/charging topics appear with CAN
- [ ] `MessageCount` appears in `MOT/IoTFoundation`
- [ ] `PayloadBytes` appears in `MOT/IoTFoundation`
- [ ] messages per minute are estimated
- [ ] topic list is reviewed before AWS-3.2
