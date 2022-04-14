#pragma once

int updateCallbacks();
int initDiscord();
int setPresence(const char* details, const char* currentState, const char* largeImageKey, const char* largeImageText, const char* smallImageKey, const char* smallImageText, int64_t startTimestamp, int64_t endTimestamp, const char* partyId, int partySize, int partySizeMax);