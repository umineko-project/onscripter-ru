/**
 *  DiscordEvents.cpp
 *  ONScripter-RU
 *
 *  Core functions for the discord api.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

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

LogLevel translateLogLevel(discord::LogLevel level) {
	switch (level) {
		case discord::LogLevel::Warn:
			return LogLevel::Warn;
			break;
		case discord::LogLevel::Error:
			return LogLevel::Error;
			break;
		default:
			return LogLevel::Info;
			break;
	}
}

void initDiscord(const char* id) {
	auto result = discord::Core::Create(strtoll(id, NULL, 10), DiscordCreateFlags_NoRequireDiscord, &core);
	state.core.reset(core);
	std::string error;
	std::string description;

	if (!state.core) {
		switch (result) {
			case discord::Result::ServiceUnavailable:
				error       = "ServiceUnavailable";
				description = "Discord isn't working";
				break;
			case discord::Result::InvalidVersion:
				error       = "InvalidVersion";
				description = "the SDK version may be outdated";
				break;
			case discord::Result::LockFailed:
				error       = "LockFailed";
				description = "an internal error on transactional operations";
				break;
			case discord::Result::InternalError:
				error       = "InternalError";
				description = "something on our side went wrong";
				break;
			case discord::Result::InvalidPayload:
				error       = "InvalidPayload";
				description = "the data you sent didn't match what we expect";
				break;
			case discord::Result::InvalidCommand:
				error       = "InvalidCommand";
				description = "that's not a thing you can do";
				break;
			case discord::Result::InvalidPermissions:
				error       = "InvalidPermissions";
				description = "you aren't authorized to do that";
				break;
			case discord::Result::NotFetched:
				error       = "NotFetched";
				description = "couldn't fetch what you wanted";
				break;
			case discord::Result::NotFound:
				error       = "NotFound";
				description = "what you're looking for doesn't exist";
				break;
			case discord::Result::Conflict:
				error       = "Conflict";
				description = "user already has a network connection open on that channel";
				break;
			case discord::Result::InvalidSecret:
				error       = "InvalidSecret";
				description = "activity secrets must be unique and not match party id";
				break;
			case discord::Result::InvalidJoinSecret:
				error       = "InvalidJoinSecret";
				description = "join request for that user does not exist";
				break;
			case discord::Result::NoEligibleActivity:
				error       = "NoEligibleActivity";
				description = "you accidentally set an ApplicationId in your UpdateActivity() payload";
				break;
			case discord::Result::InvalidInvite:
				error       = "InvalidInvite";
				description = "your game invite is no longer valid";
				break;
			case discord::Result::NotAuthenticated:
				error       = "NotAuthenticated";
				description = "the internal auth call failed for the user, and you can't do this";
				break;
			case discord::Result::InvalidAccessToken:
				error       = "InvalidAccessToken";
				description = "the user's bearer token is invalid";
				break;
			case discord::Result::ApplicationMismatch:
				error       = "ApplicationMismatch";
				description = "access token belongs to another application";
				break;
			case discord::Result::InvalidDataUrl:
				error       = "InvalidDataUrl";
				description = "something internally went wrong fetching image data";
				break;
			case discord::Result::InvalidBase64:
				error       = "InvalidBase64";
				description = "not valid Base64 data";
				break;
			case discord::Result::NotFiltered:
				error       = "NotFiltered";
				description = "you're trying to access the list before creating a stable list with Filter()";
				break;
			case discord::Result::LobbyFull:
				error       = "LobbyFull";
				description = "the lobby is full";
				break;
			case discord::Result::InvalidLobbySecret:
				error       = "InvalidLobbySecret";
				description = "the secret you're using to connect is wrong";
				break;
			case discord::Result::InvalidFilename:
				error       = "InvalidFilename";
				description = "file name is too long";
				break;
			case discord::Result::InvalidFileSize:
				error       = "InvalidFileSize";
				description = "file is too large";
				break;
			case discord::Result::InvalidEntitlement:
				error       = "InvalidEntitlement";
				description = "the user does not have the right entitlement for this game";
				break;
			case discord::Result::NotInstalled:
				error       = "NotInstalled";
				description = "Discord is not installed";
				break;
			case discord::Result::NotRunning:
				error       = "NotRunning";
				description = "Discord is not running";
				break;
			case discord::Result::InsufficientBuffer:
				error       = "InsufficientBuffer";
				description = "insufficient buffer space when trying to write";
				break;
			case discord::Result::PurchaseCanceled:
				error       = "PurchaseCanceled";
				description = "user cancelled the purchase flow";
				break;
			case discord::Result::InvalidGuild:
				error       = "InvalidGuild";
				description = "Discord guild does not exist";
				break;
			case discord::Result::InvalidEvent:
				error       = "InvalidEvent";
				description = "the event you're trying to subscribe to does not exist";
				break;
			case discord::Result::InvalidChannel:
				error       = "InvalidChannel";
				description = "Discord channel does not exist";
				break;
			case discord::Result::InvalidOrigin:
				error       = "InvalidOrigin";
				description = "the origin header on the socket does not match what you've registered (you should not see this)";
				break;
			case discord::Result::RateLimited:
				error       = "RateLimited";
				description = "you are calling that method too quickly";
				break;
			case discord::Result::OAuth2Error:
				error       = "OAuth2Error";
				description = "the OAuth2 process failed at some point";
				break;
			case discord::Result::SelectChannelTimeout:
				error       = "SelectChannelTimeout";
				description = "the user took too long selecting a channel for an invite";
				break;
			case discord::Result::GetGuildTimeout:
				error       = "GetGuildTimeout";
				description = "took too long trying to fetch the guild";
				break;
			case discord::Result::SelectVoiceForceRequired:
				error       = "SelectVoiceForceRequired";
				description = "push to talk is required for this channel";
				break;
			case discord::Result::CaptureShortcutAlreadyListening:
				error       = "CaptureShortcutAlreadyListening";
				description = "that push to talk shortcut is already registered";
				break;
			case discord::Result::UnauthorizedForAchievement:
				error       = "UnauthorizedForAchievement";
				description = "your application cannot update this achievement";
				break;
			case discord::Result::InvalidGiftCode:
				error       = "InvalidGiftCode";
				description = "the gift code is not valid";
				break;
			case discord::Result::PurchaseError:
				error       = "PurchaseError";
				description = "something went wrong during the purchase flow";
				break;
			case discord::Result::TransactionAborted:
				error       = "TransactionAborted";
				description = "purchase flow aborted because the SDK is being torn down";
				break;
			default:
				error       = "Unknown";
				description = "unknown error";
				break;
		}

		sendToLog(LogLevel::Error, "Discord error: %s, description: %s\n", error.c_str(), description.c_str());

		// std::exit(-1);
		// shutdownDiscord();
	}
	state.core->SetLogHook(
	    discord::LogLevel::Debug, [](discord::LogLevel level, const char* message) {
		    sendToLog(translateLogLevel(level), "Discord: %s\n", message);
	    });
}

void setPresence(const char* details, const char* currentState, const char* largeImageKey, const char* largeImageText, const char* smallImageKey, const char* smallImageText, const char* startTimestamp, const char* endTimestamp = NULL) {
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

void shutdownDiscord() {
	state.core->ActivityManager().ClearActivity([](discord::Result result) {
		sendToLog(((result == discord::Result::Ok) ? LogLevel::Info : LogLevel::Error), "Stopping discord!\n");
	});
}

#endif