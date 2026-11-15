# Independent review: coordinated loopback rematch

The TCP contract reviewer exercised real loopback sockets in pinned Godot:
77 checks covering both roles' explicit consent, delayed confirmation,
fragmented frames, invalid settings, duplicate messages, a fresh identity,
setup cancel and active disconnect. This exercises lobby states without the
lower-level UDP matches. The reviewer found that automatically closing on
TCP EOF could cancel a peer still finishing over UDP. The integrated client
now lets that native state continue and archives a confirmed replay even
if the lobby already detached. An integrated terminal loss fixture remains
open; this fix is source-reviewed.

The replay reviewer inspected the first guest offer, both round-one
outcomes, and the next guest offer at 1280×720. The offer displayed seed 7
and delay 2 even though the guest launched with seed 99 and delay 11. The
defeat and victory instruction lines were fully visible. The next offer
displayed seed 8 and delay 3 while the first defeat remained on screen.
The offer text on that screen is small; some live unit health bars still
draw over the victory banner. The setup overlay also leaves irrelevant
economy controls visible underneath. The prior retained November 8 frames
did not reproduce the reported clipping at this resolution.

The reviewer separately replayed the first round in Debug and Release at
7,144 and 7,145 ticks, compared every observed trace row, and matched both
separately saved archives to both VFRs. The first winner is player 0 at
7,145, hash `14de9413d247dd2a`; the preceding native tick has no winner.
Evidence: `artifacts/critic-rematch-2026-11-15/r1-summary.json`.

The same reviewer independently verified round two, including both actual
terminal PNGs, frozen package fingerprints, all four post-run archives,
unchanged process/UDP identities, ordered capture/start timestamps, and
different session IDs. Debug and Release match every observed trace row;
there is no winner at 7,087 and player 0 wins at 7,088 with hash
`c8e5299259527d3a`. Both final banners and action lines fit. Health bars
still overlap the host banner. Evidence:
`artifacts/critic-rematch-2026-11-15/r2-summary.json`.

Verdict: pass for two scripted, rendered loopback economy matches and
the replay-preserving rematch transition. These are fixed input routes
with a holding defender. They provide no human, LAN/Internet, balance,
large battle performance, production art, or shipping acceptance.
