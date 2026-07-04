# UI History Cleanup – v1.0.2

The old static `Verlauf (SoC)` card in the overview has been removed.

Reason:

- It duplicated the new History section.
- It used a static/simple chart presentation.
- It consumed too much vertical space on mobile.
- The new History section supports stored IndexedDB data, SoC, Speed and range selection.

The dashboard now uses a single central History section.
