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

int setPresence() {
  discord::Activity activity{};
  activity.SetDetails("Fruit Tarts");
  activity.SetState("Pop Snacks");
  activity.GetAssets().SetSmallImage("the");
  activity.GetAssets().SetSmallText("i mage");
  activity.GetAssets().SetLargeImage("the");
  activity.GetAssets().SetLargeText("u mage");
  activity.SetType(discord::ActivityType::Playing);
  state.core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
      std::cout << ((result == discord::Result::Ok) ? "Succeeded" : "Failed")
                << " updating activity!\n";
  });
  return 0;
}