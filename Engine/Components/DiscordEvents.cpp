#if defined(DISCORD)
#include <discord/discord.h>
#include "Support/FileIO.hpp"

struct DiscordState {
    discord::User currentUser;

    std::unique_ptr<discord::Core> core;
};

DiscordState state{};
discord::Core* core{};

void initDiscord(int32_t id) {
  auto result = discord::Core::Create(id, DiscordCreateFlags_NoRequireDiscord, &core);
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

void setPresence(const char* details, const char* currentState, const char* largeImageKey, const char* largeImageText, const char* smallImageKey, const char* smallImageText, int64_t startTimestamp, int64_t endTimestamp, const char* partyId, int partySize, int partySizeMax) {
  discord::Activity activity{};
  activity.SetDetails(details);
  activity.SetState(currentState);
  activity.GetAssets().SetSmallImage(smallImageKey);
  activity.GetAssets().SetSmallText(smallImageText);
  activity.GetAssets().SetLargeImage(largeImageKey);
  activity.GetAssets().SetLargeText(largeImageText);
  activity.GetTimestamps().SetStart(startTimestamp);
  activity.GetTimestamps().SetEnd(endTimestamp);
  activity.GetParty().GetSize().SetCurrentSize(partySize);
  activity.GetParty().GetSize().SetMaxSize(partySizeMax);
  activity.SetType(discord::ActivityType::Playing);
  state.core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
      sendToLog(((result == discord::Result::Ok) ? LogLevel::Error : LogLevel::Info), " updating activity!\n");
  });
}

void runDiscordCallbacks() {
  sendToLog(LogLevel::Error, "ratiod");
  sendToLog(LogLevel::Info, (const char*) state.core->RunCallbacks());
}
#endif