# Release process

```bash
git status
git add .
git commit -m "release: stabilize LilyGO LTE MQTT integration"

git tag -a v1.1.0-lilygo-stability \
  -m "v1.1.0 LilyGO stability release: LTE MQTT, GPS, CAN, backup/restore"

git push origin develop
git push origin v1.1.0-lilygo-stability
```

Merge to main after validation:

```bash
git checkout main
git pull origin main
git merge --no-ff develop
git push origin main
```
