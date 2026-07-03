# Configuration

MOT separates display names from technical IDs.

| Field | Purpose | Example |
|---|---|---|
| Vehicle Name | Human-readable name | Microlino Pioneer |
| Vehicle ID | MQTT topic component | pioneer |
| MQTT Prefix | Root topic | mot |

The resulting topic root is:

```text
mot/pioneer
```

Never commit personal broker hosts, usernames or passwords.
