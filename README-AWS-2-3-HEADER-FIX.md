# AWS-2.3 dashboard header correction

The first AWS-2.3 package was generated from an older dashboard template.
That restored the former single MQTT status pill and removed the separate
Microlino/OBD2 presence block.

This corrected package combines:

- the current two-part MQTT + Microlino/OBD2 header,
- the current mobile network/IP row,
- the current sidebar network information,
- the new AWS-2.3 data-provider boundary.

No visible status feature is intentionally removed.
