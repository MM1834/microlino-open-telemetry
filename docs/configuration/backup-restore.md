# Configuration Backup and Restore

## Backup

Configuration can be exported as JSON.

```text
GET /api/config/export
```

## Restore

Configuration can be restored from the Web UI by pasting or selecting a backup JSON file.

Restore validates:

- non-empty JSON
- valid JSON syntax

After restore, the device saves the configuration and reboots.

## Factory reset

Factory reset clears the configuration and reboots. The Web UI asks for confirmation before executing the reset.
