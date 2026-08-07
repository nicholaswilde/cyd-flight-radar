#pragma once

namespace ui {

/** Draw the static sonar/radar grid (black disc, green overlay, labels). */
void radarDisplayDraw();

/** Redraw aircraft only (blits cached grid; no full-screen clear). */
void radarDisplayUpdateAnimation();
void radarDisplayRefreshAircraft();
void radarDisplaySetStale(bool is_stale);
void updateThemeColors();

bool radarDisplayHandleTouch(int x, int y);

}  // namespace ui
