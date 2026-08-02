#ifndef UI_SETTINGS_MENU_H
#define UI_SETTINGS_MENU_H

#include <stdbool.h>

namespace ui::settings {

void setup();
void loop();
void show();
void hide();
bool isVisible();
bool isAirportsEnabled();
bool isMediumAirportsEnabled();
bool isGroundAircraftEnabled();
bool isRadarSweepEnabled();
bool isAutoDimmingEnabled();
int getMaxAltitudeFilter();
int getSweepRotationSpeedMs();
bool isTimezoneChanged();
void clearTimezoneChanged();
const char* getTimezoneStr();

} // namespace ui::settings

#endif // UI_SETTINGS_MENU_H
