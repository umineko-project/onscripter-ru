/**
 *  DiscordEvents.hpp
 *  ONScripter-RU
 *
 *  Core functions for the discord api.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

void initDiscord(const char* id);
void setPresence(const char* details, const char* currentState, const char* largeImageKey, const char* largeImageText, const char* smallImageKey, const char* smallImageText, const char* startTimestamp, const char* endTimestamp = NULL);
void runDiscordCallbacks();
void shutdownDiscord();