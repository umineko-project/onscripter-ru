/**
 *  UIKitWrapper.hpp
 *  ONScripter-RU
 *
 *  Implements uikit-specific interfaces.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

bool backupDisable(const char *path);
bool openURL(const char *url);

union SDL_Event;
using KeySender = void (*)(SDL_Event *);
void setupKeyboardHandling(KeySender s);
