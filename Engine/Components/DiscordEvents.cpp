#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <discord/discord.h>

struct DiscordState {
    discord::User currentUser;

    std::unique_ptr<discord::Core> core;
};

DiscordState state{};
discord::Core* core{};

int updateCallbacks() {
  
  state.core->RunCallbacks();
  return 0;
};

int initDiscord() {
  auto result = discord::Core::Create(933377421372182550, DiscordCreateFlags_NoRequireDiscord, &core);
  state.core.reset(core);
  if (!state.core) {
      std::cout << "Failed to instantiate discord core! (err " << static_cast<int>(result)
                << ")\n";
      std::exit(-1);
  }
  state.core->SetLogHook(
    discord::LogLevel::Debug, [](discord::LogLevel level, const char* message) {
        std::cerr << "Log(" << static_cast<uint32_t>(level) << "): " << message << "\n";
    });
  return 0;
}

int setPresence(const char* details, const char* currentState, const char* largeImageKey, const char* largeImageText, const char* smallImageKey, const char* smallImageText, int64_t startTimestamp, int64_t endTimestamp, const char* partyId, int partySize, int partySizeMax) {
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
      std::cout << ((result == discord::Result::Ok) ? "Succeeded" : "Failed")
                << " updating activity!\n";
  });
  return 0;
}