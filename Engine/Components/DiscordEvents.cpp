int setPresence(char* details, char* currentState, char* smallImageKey, char* smallImageText, char* largeImageKey, char* largeImageText) {
  discord::Activity activity{};
  activity.SetDetails(details);
  activity.SetState(currentState);
  activity.GetAssets().SetSmallImage(smallImageKey);
  activity.GetAssets().SetSmallText(smallImageText);
  activity.GetAssets().SetLargeImage(largeImageKey);
  activity.GetAssets().SetLargeText(largeImageText);
  activity.SetType(discord::ActivityType::Playing);
  state.core->ActivityManager().UpdateActivity(activity, [](discord::Result result) {
      std::cout << ((result == discord::Result::Ok) ? "Succeeded" : "Failed")
                << " updating activity!\n";
  });
  return 0;
} 