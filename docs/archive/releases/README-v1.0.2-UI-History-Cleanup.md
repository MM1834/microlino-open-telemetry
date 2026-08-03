# MOT v1.0.2 – UI History Cleanup

Entfernt den alten oberen `Verlauf (SoC)`-Block und ergänzt einen History-Link im Menü.

## Einspielen

```bash
unzip ~/Downloads/mot-v1.0.2-ui-history-cleanup.zip
cp -R mot-v1.0.2-ui-history-cleanup/. .
rm -rf mot-v1.0.2-ui-history-cleanup

python3 tools/apply_v1_0_2_ui_history_cleanup.py
git diff
```
