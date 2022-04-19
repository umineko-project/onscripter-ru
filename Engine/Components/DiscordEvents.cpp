#if defined(DISCORD)
#include <discord/discord.h>
#include "Support/FileIO.hpp"
#include <iostream>
struct DiscordState {
    discord::User currentUser;

    std::unique_ptr<discord::Core> core;
};

DiscordState state{};
discord::Core* core{};

void initDiscord(const char* id) {
  auto result = discord::Core::Create(strtoll(id, NULL, 10), DiscordCreateFlags_NoRequireDiscord, &core);
  state.core.reset(core);
  if (!state.core) {
      sendToLog(LogLevel::Error, "%d\n", static_cast<int>(result));
      std::exit(-1);
  }
  state.core->SetLogHook(
    discord::LogLevel::Debug, [](discord::LogLevel level, const char* message) {
        sendToLog(LogLevel::Error, "Log( %d) %s\n", static_cast<uint32_t>(level), message);
    });
}

void setPresence(const char* details, const char* currentState, const char* largeImageKey, const char* largeImageText, const char* smallImageKey, const char* smallImageText, const char* startTimestamp, const char* endTimestamp) {
  discord::Activity activity{};
  activity.SetDetails(details);
  activity.SetState(currentState);
  activity.GetAssets().SetSmallImage(smallImageKey);
  activity.GetAssets().SetSmallText(smallImageText);
  activity.GetAssets().SetLargeImage(largeImageKey);
  activity.GetAssets().SetLargeText(largeImageText);
  activity.GetTimestamps().SetStart(strtoll(startTimestamp, NULL, 10));
  activity.GetTimestamps().SetEnd(strtoll(endTimestamp, NULL, 10));
  activity.SetType(discord::ActivityType::Playing);
  state.core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
      sendToLog(((result == discord::Result::Ok) ? LogLevel::Info : LogLevel::Error), "Updating activity!\n");
  });
}

void runDiscordCallbacks() {
  state.core->RunCallbacks();
}
#endif