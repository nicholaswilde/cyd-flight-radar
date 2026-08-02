#ifndef UI_SETTINGS_MENU_H
#define UI_SETTINGS_MENU_H

#include <stdbool.h>

namespace ui::settings {

void setup();
void loop();
void show();
void hide();
bool isVisible();

} // namespace ui::settings

#endif // UI_SETTINGS_MENU_H
