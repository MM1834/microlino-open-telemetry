# MOT Dashboard Official Vehicle Image Patch

This patch replaces the temporary dashboard vehicle illustration with the approved Microlino image.

## Files

- `dashboard/img/microlino.jpeg` — approved vehicle image used by the dashboard
- `dashboard/index.html` — now references `img/microlino.jpeg`
- `dashboard/css/dashboard.css` — minor styling for the vehicle image
- `docs/assets/microlino.jpeg` — reusable image for README/docs

## Apply

From the repository root:

```bash
unzip ~/Downloads/mot-dashboard-v1.0-official-image.zip
cp -R mot-dashboard-v1.0-official-image/. .
rm -rf mot-dashboard-v1.0-official-image

git add .
git commit -m "Use approved Microlino dashboard image"
git push
```

After deployment, hard-refresh the browser or open the dashboard with a cache buster, for example:

```text
https://your-domain.example/MOT/dashboard/?v=official-image
```
