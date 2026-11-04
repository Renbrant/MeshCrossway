# v0.9.13 node card layout

v0.9.13 is an experimental firmware candidate. The hardware-tested stable release remains v0.8.26.

The v0.9.12 screenshots show node names beside metadata that can wrap to multiple lines. Those text areas occupied the same vertical space, causing names, heard times and message totals to collide.

Node cards now use three fixed rows: node name, heard time plus unread count, and traffic total plus optional distance. Each label has a fixed height and single-line ellipsis, and each card has a fixed height with spacing between rows. The same layout applies to Meshtastic and MeshCore nodes.

The ESP-IDF build and automated preflight should pass before publication. Hardware confirmation is still required before considering this release stable.
