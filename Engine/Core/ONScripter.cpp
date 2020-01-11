/**
 *  ONScripter.cpp
 *  ONScripter-RU
 *
 *  Core execution block parser.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Core/ONScripter.hpp"
#include "Engine/Components/Async.hpp"
#include "Engine/Components/Joystick.hpp"
#include "Engine/Components/Fonts.hpp"
#include "Engine/Components/Window.hpp"
#include "Engine/Graphics/Common.hpp"
#include "Engine/Media/Controller.hpp"
#include "Engine/Layers/Media.hpp"
#include "Resources/Support/Resources.hpp"
#include "Resources/Support/Version.hpp"
#include "Support/FileIO.hpp"

#ifdef USE_OBJC
#ifdef MACOSX
#include "Support/Apple/CocoaWrapper.hpp"
#else
#include "Support/Apple/UIKitWrapper.hpp"
#endif
#endif

ONScripter ons;

extern "C" void waveCallback(int channel);

const char REGISTRY_FILE[]    = "registry.txt";
const char DLL_FILE[]         = "dll.txt";
const char DEFAULT_ENV_FONT[] = "MS Gothic"; //haeleth change to use English-language font name

const char DEFAULT_WM_TITLE[] = "";
#ifdef MACOSX
const int DEFAULT_WM_ICON_W = 128;
const int DEFAULT_WM_ICON_H = 128;
#elif defined(WIN32)
const int DEFAULT_WM_ICON_W = 32;
const int DEFAULT_WM_ICON_H = 32;
#endif
const int DEFAULT_FONT_SIZE = 26;

using CommandFunc = int (ONScripter::*)();
static std::unordered_map<HashedString, CommandFunc> func_lut{
    {"z_order_override2", &ONScripter::zOrderOverrideCommand},
    {"z_order_override", &ONScripter::zOrderOverrideCommand},
    {"z_override_preserve", &ONScripter::zOrderOverridePreserveCommand},
    {"wheelvalue", &ONScripter::wheelvalueCommand},
    {"wait2", &ONScripter::waitCommand},
    {"waitlips", &ONScripter::waitlipsCommand},
    {"waits", &ONScripter::waitCommand},
    {"waitvoice", &ONScripter::waitvoiceCommand},
    {"waitvideo", &ONScripter::waitvideoCommand},
    {"wait_on_d", &ONScripter::waitOnDialogueCommand},
    {"vv_setlog", &ONScripter::vvSetLogCommand},
    {"videovol", &ONScripter::videovolCommand},
    {"video", &ONScripter::movieCommand},
    {"verify_files", &ONScripter::verifyFilesCommand},
    {"tree_setra", &ONScripter::treeSetCommand},
    {"tree_setr", &ONScripter::treeSetCommand},
    {"tree_seta", &ONScripter::treeSetCommand},
    {"tree_set", &ONScripter::treeSetCommand},
    {"tree_exec", &ONScripter::treeExecuteCommand},
    {"tree_get", &ONScripter::treeGetCommand},
    {"tree_clear", &ONScripter::treeClearCommand},
    {"text_speed_t", &ONScripter::textDisplaySpeedCommand},
    {"text_fade_t", &ONScripter::textFadeDurationCommand},
    {"text_speed", &ONScripter::textDisplaySpeedCommand},
    {"text_fade", &ONScripter::textFadeDurationCommand},
    {"textatlas", &ONScripter::textAtlasCommand},
    {"texec3", &ONScripter::texecCommand},
    {"talsp", &ONScripter::talCommand},
    {"ssa_stop", &ONScripter::subtitleStopCommand},
    {"ssa_load", &ONScripter::subtitleLoadCommand},
    {"ssa_font", &ONScripter::subtitleFontCommand},
    {"stopwatch", &ONScripter::stopwatchCommand},
    {"stopvideo", &ONScripter::movieCommand},
    {"setlog", &ONScripter::setLogCommand},
    {"aspt2", &ONScripter::spritePropertyCommand},
    {"aspt", &ONScripter::spritePropertyCommand},
    {"spt2", &ONScripter::spritePropertyCommand},
    {"spt", &ONScripter::spritePropertyCommand},
    {"spriteset_pos", &ONScripter::spritesetPosCommand},
    {"spriteset_mask", &ONScripter::spritesetMaskCommand},
    {"spriteset_enable", &ONScripter::spritesetEnableCommand},
    {"spriteset_blur", &ONScripter::spritesetBlurCommand},
    {"spriteset_alpha", &ONScripter::spritesetAlphaCommand},
    {"sptwait2", &ONScripter::spritePropertyWaitCommand},
    {"sptwait", &ONScripter::spritePropertyWaitCommand},
    {"snaplog", &ONScripter::snapLogCommand},
    {"scroll_exceeds2", &ONScripter::scrollExceedsCommand},
    {"scroll_exceeds", &ONScripter::scrollExceedsCommand},
    {"scrollable_scroll", &ONScripter::scrollableScrollCommand},
    {"scrollable_get_hovered_elem", &ONScripter::scrollableGetHoveredElementCommand},
    {"scrollable_display", &ONScripter::scrollableDisplayCommand},
    {"scrollable_cfg", &ONScripter::scrollableConfigCommand},
    {"scrollable", &ONScripter::scrollableSpriteCommand},
    {"savereset", &ONScripter::saveresetCommand},
    {"gptwait", &ONScripter::globalPropertyWaitCommand},
    {"agpt", &ONScripter::globalPropertyCommand},
    {"gpt", &ONScripter::globalPropertyCommand},
    {"goto", &ONScripter::gotoCommand},
    {"getscriptpath", &ONScripter::getScriptPathCommand},
    {"getscriptnum", &ONScripter::getScriptNumCommand},
    {"getvideovol", &ONScripter::getvideovolCommand},
    {"getrenderername", &ONScripter::getRendererNameCommand},
    {"getrenderernum", &ONScripter::getRendererNumCommand},
    {"getlog2", &ONScripter::getlogCommand},
    {"sskip", &ONScripter::superSkipCommand},
    {"sskip_unset", &ONScripter::superSkipUnsetCommand},
    {"spritesetptwait", &ONScripter::spritesetPropertyWaitCommand},
    {"aspritesetpt", &ONScripter::spritesetPropertyCommand},
    {"spritesetpt", &ONScripter::spritesetPropertyCommand},
    {"splitstring", &ONScripter::splitCommand},
    {"smart_quotes", &ONScripter::smartquotesCommand},
    {"skip_unread", &ONScripter::skipUnreadCommand},
    {"skip_enable", &ONScripter::skipModeCommand},
    {"skip_disable", &ONScripter::skipModeCommand},
    {"setvoicewaitmul", &ONScripter::setVoiceWaitMulCommand},
    {"set_scale_center", &ONScripter::setScaleCenterCommand},
    {"set_hotspot", &ONScripter::setHotspotCommand},
    {"setwindowd_name_padding", &ONScripter::setwindowDynamicNamePaddingCommand},
    {"setwindowd_padding", &ONScripter::setwindowDynamicPaddingCommand},
    {"setwindowd_dimensions", &ONScripter::setwindowDynamicDimensionsCommand},
    {"setwindowd_main_region", &ONScripter::setwindowDynamicMainRegionCommand},
    {"setwindowd_non_region", &ONScripter::setwindowDynamicNoNameRegionCommand},
    {"setwindowd_name_region", &ONScripter::setwindowDynamicNameRegionCommand},
    {"setwindowd_off", &ONScripter::setwindowDynamicCommand},
    {"setwindowd", &ONScripter::setwindowDynamicCommand},
    {"setwindow5", &ONScripter::setwindow2Command},
    {"setwindow4name", &ONScripter::setwindow4Command},
    {"setwindow4", &ONScripter::setwindow4Command},
    {"setfps", &ONScripter::setFpsCommand},
    {"scriptmute", &ONScripter::scriptMuteCommand},
    {"screenflip", &ONScripter::screenFlipCommand},
    {"rumble", &ONScripter::rumbleCommand},
    {"relaunch", &ONScripter::relaunchCommand},
    {"regex_define", &ONScripter::regexDefineCommand},
    {"quakex_t", &ONScripter::quakeApiCommand},
    {"quakey_t", &ONScripter::quakeApiCommand},
    {"quake_t", &ONScripter::quakeApiCommand},
    {"quake_off", &ONScripter::quakeendCommand},
    {"profilestop", &ONScripter::profilestopCommand},
    {"profilestart", &ONScripter::profilestartCommand},
    {"print2", &ONScripter::printCommand},
    {"preset_define", &ONScripter::presetdefineCommand},
    {"past_log", &ONScripter::pastLogCommand},
    {"past_label2", &ONScripter::pastLabelCommand},
    {"past_label", &ONScripter::pastLabelCommand},
    {"operate_config", &ONScripter::operateConfigCommand},
    {"no_smart_quotes", &ONScripter::nosmartquotesCommand},
    {"nearest_log", &ONScripter::nearestJumpableLogEntryIndexCommand},
    {"moreram", &ONScripter::moreramCommand},
    {"mark_read", &ONScripter::markReadCommand},
    {"mark_range_read", &ONScripter::markRangeReadCommand},
    {"mark_all_read", &ONScripter::markAllReadCommand},
    {"main_goto", &ONScripter::mainGotoCommand},
    {"main_label", &ONScripter::mainLabelCommand},
    {"make_choice", &ONScripter::makeChoiceCommand},
    {"lv_stop", &ONScripter::lvStopCommand},
    {"lv_setlog", &ONScripter::lvSetLogCommand},
    {"lv_play", &ONScripter::lvPlayCommand},
    {"lsph2mul", &ONScripter::lsp2Command},
    {"lsp2mul", &ONScripter::lsp2Command},
    {"lbsp2", &ONScripter::lsp2Command},
    {"lookahead", &ONScripter::lookaheadCommand},
    {"loadreg", &ONScripter::loadregCommand},
    {"loadfromreg", &ONScripter::loadfromregCommand},
    {"lips_sprite", &ONScripter::lipsSpriteCommand},
    {"lips_lims", &ONScripter::lipsLimitsCommand},
    {"lips_channel", &ONScripter::lipsChannelCommand},
    {"jauto", &ONScripter::jautomodeCommand},
    {"jnauto", &ONScripter::jnautomodeCommand},
    {"jskip_s", &ONScripter::jskipSuperCommand},
    {"jnskip_s", &ONScripter::jnskipSuperCommand},
    {"jskip", &ONScripter::jskipCommand},
    {"jnskip", &ONScripter::jnskipCommand},
    {"italic_font", &ONScripter::aliasFontCommand},
    {"ignore_voicedelay", &ONScripter::ignoreVoiceDelayCommand},
    {"hyphen_carry", &ONScripter::hyphenCarryCommand},
    {"getram", &ONScripter::getramCommand},
    {"get_cvs", &ONScripter::getChoiceVectorSizeCommand},
    {"get_log_data", &ONScripter::getLogDataCommand},
    {"get_unique_log_entry_index", &ONScripter::getUniqueLogEntryIndexCommand},
    {"fall", &ONScripter::fallCommand},
    {"error", &ONScripter::errorCommand},
    {"enable_transitions2", &ONScripter::enableTransitionsCommand},
    {"enable_transitions", &ONScripter::enableTransitionsCommand},
    {"enable_lsp_gradients", &ONScripter::useTextGradientsForSpritesCommand},
    {"enable_gradients", &ONScripter::useTextGradientsCommand},
    {"enable_custom_cursors", &ONScripter::enableCustomCursors},
    {"drop_cache_img", &ONScripter::dropCacheCommand},
    {"displayscreenshot", &ONScripter::displayScreenshotCommand},
    {"d_setvoicewait", &ONScripter::dialogueSetVoiceWaitCommand},
    {"d_dispose", &ONScripter::disposeDialogueCommand},
    {"d_condition", &ONScripter::conditionDialogueCommand},
    {"d_continue", &ONScripter::dialogueContinueCommand},
    {"d_add_ends", &ONScripter::dialogueAddEndsCommand},
    {"debug_string", &ONScripter::debugStrCommand},
    {"custom_cursor", &ONScripter::customCursorCommand},
    {"count_symbols", &ONScripter::countSymbolsCommand},
    {"color_mod2", &ONScripter::colorModCommand},
    {"color_mod", &ONScripter::colorModCommand},
    {"clearlog", &ONScripter::clearLogCommand},
    {"clear_cache_img", &ONScripter::clearCacheCommand},
    {"clear_cache_snd", &ONScripter::clearCacheCommand},
    {"savechoices", &ONScripter::choicesToStringCommand},
    {"loadchoices", &ONScripter::choicesFromStringCommand},
    {"child_image_detach2", &ONScripter::childImageDetachCommand},
    {"child_image_detach", &ONScripter::childImageDetachCommand},
    {"child_image2", &ONScripter::childImageCommand},
    {"child_image", &ONScripter::childImageCommand},
    {"change_font", &ONScripter::changeFontCommand},
    {"cell2", &ONScripter::cellCommand},
    {"cache_slot_snd", &ONScripter::cacheSlotTypeCommand},
    {"cache_slot_img", &ONScripter::cacheSlotTypeCommand},
    {"cache_snd", &ONScripter::loadCacheCommand},
    {"cache_img", &ONScripter::loadCacheCommand},
    {"border_padding", &ONScripter::borderPaddingCommand},
    {"bold_italic_font", &ONScripter::aliasFontCommand},
    {"bold_font", &ONScripter::aliasFontCommand},
    {"blur", &ONScripter::blurCommand},
    {"blend_mode2", &ONScripter::blendModeCommand},
    {"blend_mode", &ONScripter::blendModeCommand},
    {"async_cache_snd", &ONScripter::asyncLoadCacheCommand},
    {"async_cache_img", &ONScripter::asyncLoadCacheCommand},
    {"bgm_wait", &ONScripter::bgmPropertyWaitCommand},
    {"backup_disable", &ONScripter::backupDisableCommand},
    {"ch_wait", &ONScripter::mixChannelPropertyWaitCommand},
    {"btnhover_d", &ONScripter::btnhover_dCommand},
    {"abgm_prop", &ONScripter::bgmPropertyCommand},
    {"ach_prop", &ONScripter::mixChannelPropertyCommand},
    {"api_compat", &ONScripter::apiCompatCommand},
    {"accept_choice", &ONScripter::acceptChoiceCommand},
    {"accept_choice_next_index", &ONScripter::acceptChoiceNextIndexCommand},
    {"accept_choice_vector_size", &ONScripter::acceptChoiceVectorSizeCommand},
    {"atomic", &ONScripter::atomicCommand},

    //Undocumented?
    {"sp_rgb_gradation", &ONScripter::sp_rgb_gradationCommand},

    {"yesnobox", &ONScripter::yesnoboxCommand},
    {"wavestop", &ONScripter::wavestopCommand},
    {"waveloop", &ONScripter::waveCommand},
    {"wave", &ONScripter::waveCommand},
    {"waittimer", &ONScripter::waittimerCommand},
    {"wait", &ONScripter::waitCommand},
    {"vsp2", &ONScripter::vspCommand},
    {"vsp", &ONScripter::vspCommand},
    {"voicevol", &ONScripter::voicevolCommand},
    {"trap", &ONScripter::trapCommand},
    {"transbtn", &ONScripter::transbtnCommand},
    {"textspeeddefault", &ONScripter::textspeeddefaultCommand},
    {"textspeed", &ONScripter::textspeedCommand},
    {"textshow", &ONScripter::textshowCommand},
    {"texton", &ONScripter::textonCommand},
    {"textoff2", &ONScripter::textoffCommand},
    {"textoff", &ONScripter::textoffCommand},
    {"texthide", &ONScripter::texthideCommand},
    {"textexbtn", &ONScripter::textexbtnCommand},
    {"textclear", &ONScripter::textclearCommand},
    {"textbtnwait", &ONScripter::btnwaitCommand},
    {"textbtnstart", &ONScripter::textbtnstartCommand},
    {"textbtnoff", &ONScripter::textbtnoffCommand},
    {"texec", &ONScripter::texecCommand},
    {"tateyoko", &ONScripter::tateyokoCommand},
    {"tal", &ONScripter::talCommand},
    {"tablegoto", &ONScripter::tablegotoCommand},
    {"systemcall", &ONScripter::systemcallCommand},
    {"strsph", &ONScripter::strspCommand},
    {"strsp", &ONScripter::strspCommand},
    {"stop", &ONScripter::stopCommand},
    {"spstr", &ONScripter::spstrCommand},
    {"spreload", &ONScripter::spreloadCommand},
    {"split", &ONScripter::splitCommand},
    {"spclclk", &ONScripter::spclclkCommand},
    {"spbtn", &ONScripter::spbtnCommand},
    {"skipoff", &ONScripter::skipoffCommand},
    {"shell", &ONScripter::shellCommand},
    {"sevol", &ONScripter::sevolCommand},
    {"setwindow3", &ONScripter::setwindow3Command},
    {"setwindow2", &ONScripter::setwindow2Command},
    {"setwindow", &ONScripter::setwindowCommand},
    {"seteffectspeed", &ONScripter::seteffectspeedCommand},
    {"setcursor", &ONScripter::setcursorCommand},
    {"selnum", &ONScripter::selectCommand},
    {"selgosub", &ONScripter::selectCommand},
    {"selectbtnwait", &ONScripter::btnwaitCommand},
    {"select", &ONScripter::selectCommand},
    {"savetime", &ONScripter::savetimeCommand},
    {"savescreenshot2", &ONScripter::savescreenshotCommand},
    {"savescreenshot", &ONScripter::savescreenshotCommand},
    {"saveon", &ONScripter::saveonCommand},
    {"saveoff", &ONScripter::saveoffCommand},
    {"savegame2", &ONScripter::savegameCommand},
    {"savegame", &ONScripter::savegameCommand},
    {"savefileexist", &ONScripter::savefileexistCommand},
    {"r_trap", &ONScripter::trapCommand},
    {"rnd", &ONScripter::rndCommand},
    {"rnd2", &ONScripter::rndCommand},
    {"rmode", &ONScripter::rmodeCommand},
    {"resettimer", &ONScripter::resettimerCommand},
    {"resetmenu", &ONScripter::resetmenuCommand},
    {"reset", &ONScripter::resetCommand},
    {"repaint", &ONScripter::repaintCommand},
    {"quakey", &ONScripter::quakeApiCommand},
    {"quakex", &ONScripter::quakeApiCommand},
    {"quake", &ONScripter::quakeApiCommand},
    {"puttext", &ONScripter::puttextCommand},
    {"prnumclear", &ONScripter::prnumclearCommand},
    {"prnum", &ONScripter::prnumCommand},
    {"print", &ONScripter::printCommand},
    {"language", &ONScripter::languageCommand},
    {"playstop", &ONScripter::mp3stopCommand},
    {"playonce", &ONScripter::playCommand},
    {"play", &ONScripter::playCommand},
    {"okcancelbox", &ONScripter::yesnoboxCommand},
    {"ofscpy", &ONScripter::ofscopyCommand},
    {"ofscopy", &ONScripter::ofscopyCommand},
    {"nega", &ONScripter::negaCommand},
    {"msp2", &ONScripter::mspCommand},
    {"msp", &ONScripter::mspCommand},
    {"mpegplay", &ONScripter::movieCommand},
    {"mp3vol", &ONScripter::mp3volCommand},
    {"mp3stop", &ONScripter::mp3stopCommand},
    {"mp3save", &ONScripter::mp3Command},
    {"mp3loop", &ONScripter::mp3Command},
    {"mp3fadeout", &ONScripter::mp3fadeoutCommand},
    {"mp3fadein", &ONScripter::mp3fadeinCommand},
    {"mp3", &ONScripter::mp3Command},
    {"movie", &ONScripter::movieCommand},
    {"movemousecursor", &ONScripter::movemousecursorCommand},
    {"mousemode", &ONScripter::mousemodeCommand},
    {"monocro", &ONScripter::monocroCommand},
    {"minimizewindow", &ONScripter::minimizewindowCommand},
    {"mesbox", &ONScripter::mesboxCommand},
    {"menu_window", &ONScripter::menu_windowCommand},
    {"menu_waveon", &ONScripter::menu_waveonCommand},
    {"menu_waveoff", &ONScripter::menu_waveoffCommand},
    {"menu_full", &ONScripter::menu_fullCommand},
    {"menu_click_page", &ONScripter::menu_click_pageCommand},
    {"menu_click_def", &ONScripter::menu_click_defCommand},
    {"menu_automode", &ONScripter::menu_automodeCommand},
    {"lsph2sub", &ONScripter::lsp2Command},
    {"lsph2add", &ONScripter::lsp2Command},
    {"lsph2", &ONScripter::lsp2Command},
    {"lsph", &ONScripter::lspCommand},
    {"lsp2sub", &ONScripter::lsp2Command},
    {"lsp2add", &ONScripter::lsp2Command},
    {"lsp2", &ONScripter::lsp2Command},
    {"lsp", &ONScripter::lspCommand},
    {"lr_trap", &ONScripter::trapCommand},
    {"lrclick", &ONScripter::clickCommand},
    {"loopbgmstop", &ONScripter::loopbgmstopCommand},
    {"loopbgm", &ONScripter::loopbgmCommand},
    {"lookbackflush", &ONScripter::lookbackflushCommand},
    {"lookbackbutton", &ONScripter::lookbackbuttonCommand},
    {"logsp2", &ONScripter::logspCommand},
    {"logsp", &ONScripter::logspCommand},
    {"locate", &ONScripter::locateCommand},
    {"loadgame", &ONScripter::loadgameCommand},
    {"linkcolor", &ONScripter::linkcolorCommand},
    {"ld", &ONScripter::ldCommand},
    {"layermessage", &ONScripter::layermessageCommand},
    {"jumpf", &ONScripter::jumpfCommand},
    {"jumpb", &ONScripter::jumpbCommand},
    {"isfull", &ONScripter::isfullCommand},
    {"isskip", &ONScripter::isskipCommand},
    {"ispage", &ONScripter::ispageCommand},
    {"isdown", &ONScripter::isdownCommand},
    {"insertmenu", &ONScripter::insertmenuCommand},
    {"input", &ONScripter::inputCommand},
    {"indent", &ONScripter::indentCommand},
    {"humanorder", &ONScripter::humanorderCommand},
    {"getzxc", &ONScripter::getzxcCommand},
    {"getvoicevol", &ONScripter::getvoicevolCommand},
    {"getversion", &ONScripter::getversionCommand},
    {"gettimer", &ONScripter::gettimerCommand},
    {"gettextbtnstr", &ONScripter::gettextbtnstrCommand},
    {"gettext", &ONScripter::gettextCommand},
    {"gettaglog", &ONScripter::gettaglogCommand},
    {"gettag", &ONScripter::gettagCommand},
    {"gettab", &ONScripter::gettabCommand},
    {"getspsize2", &ONScripter::getspsizeCommand},
    {"getspsize", &ONScripter::getspsizeCommand},
    {"getspmode", &ONScripter::getspmodeCommand},
    {"getskipoff", &ONScripter::getskipoffCommand},
    {"getsevol", &ONScripter::getsevolCommand},
    {"getscreenshot", &ONScripter::getscreenshotCommand},
    {"getsavestr", &ONScripter::getsavestrCommand},
    {"getret", &ONScripter::getretCommand},
    {"getreg", &ONScripter::getregCommand},
    {"getpageup", &ONScripter::getpageupCommand},
    {"getpage", &ONScripter::getpageCommand},
    {"getnextline", &ONScripter::getcursorposCommand},
    {"getmp3vol", &ONScripter::getmp3volCommand},
    {"getmousepos", &ONScripter::getmouseposCommand},
    {"getmouseover", &ONScripter::getmouseoverCommand},
    {"getmclick", &ONScripter::getmclickCommand},
    {"getlogtext", &ONScripter::gettextCommand},
    {"getlog", &ONScripter::getlogCommand},
    {"getinsert", &ONScripter::getinsertCommand},
    {"getfunction", &ONScripter::getfunctionCommand},
    {"getenter", &ONScripter::getenterCommand},
    {"getcursorpos2", &ONScripter::getcursorposCommand},
    {"getcursorpos", &ONScripter::getcursorposCommand},
    {"getcursor", &ONScripter::getcursorCommand},
    {"getcselstr", &ONScripter::getcselstrCommand},
    {"getcselnum", &ONScripter::getcselnumCommand},
    {"getbtntimer", &ONScripter::gettimerCommand},
    {"getbgmvol", &ONScripter::getmp3volCommand},
    {"game", &ONScripter::gameCommand},
    {"flushout", &ONScripter::flushoutCommand},
    {"fileexist", &ONScripter::fileexistCommand},
    {"existspbtn", &ONScripter::spbtnCommand},
    {"exec_dll", &ONScripter::exec_dllCommand},
    {"exbtn_d", &ONScripter::exbtnCommand},
    {"exbtn", &ONScripter::exbtnCommand},
    {"erasetextwindow", &ONScripter::erasetextwindowCommand},
    {"erasetextbtn", &ONScripter::erasetextbtnCommand},
    {"effectskip", &ONScripter::effectskipCommand},
    {"end", &ONScripter::endCommand},
    {"dwavestop", &ONScripter::dwavestopCommand},
    {"dwaveplayloop", &ONScripter::dwaveCommand},
    {"dwaveplay", &ONScripter::dwaveCommand},
    {"dwaveloop", &ONScripter::dwaveCommand},
    {"dwaveload", &ONScripter::dwaveCommand},
    {"dwave", &ONScripter::dwaveCommand},
    {"drawtext", &ONScripter::drawtextCommand},
    {"drawsp3", &ONScripter::drawsp3Command},
    {"drawsp2", &ONScripter::drawsp2Command},
    {"drawsp", &ONScripter::drawspCommand},
    {"drawfill", &ONScripter::drawfillCommand},
    {"drawend", &ONScripter::drawendCommand},
    {"drawclear", &ONScripter::drawclearCommand},
    {"drawbg2", &ONScripter::drawbg2Command},
    {"drawbg", &ONScripter::drawbgCommand},
    {"draw", &ONScripter::drawCommand},
    {"deletescreenshot", &ONScripter::deletescreenshotCommand},
    {"delay", &ONScripter::delayCommand},
    {"definereset", &ONScripter::defineresetCommand},
    {"d_name_refresh", &ONScripter::dialogueNameCommand}, //ons-ru
    {"d_name", &ONScripter::dialogueNameCommand},         //ons-ru
    {"d2", &ONScripter::dialogueCommand},                 //ons-ru
    {"d", &ONScripter::dialogueCommand},                  //ons-ru
    {"csp2", &ONScripter::cspCommand},
    {"csp", &ONScripter::cspCommand},
    {"cselgoto", &ONScripter::cselgotoCommand},
    {"cselbtn", &ONScripter::cselbtnCommand},
    {"csel", &ONScripter::selectCommand},
    {"click", &ONScripter::clickCommand},
    {"cl", &ONScripter::clCommand},
    {"chvol", &ONScripter::chvolCommand},
    {"checkpage", &ONScripter::checkpageCommand},
    {"checkkey", &ONScripter::checkkeyCommand},
    {"cellcheckspbtn", &ONScripter::spbtnCommand},
    {"cellcheckexbtn", &ONScripter::exbtnCommand},
    {"cell", &ONScripter::cellCommand},
    {"caption", &ONScripter::captionCommand},
    {"btnwait2", &ONScripter::btnwaitCommand},
    {"btnwait", &ONScripter::btnwaitCommand},
    {"btntime2", &ONScripter::btntimeCommand},
    {"btntime", &ONScripter::btntimeCommand},
    {"btndown", &ONScripter::btndownCommand},
    {"btndef", &ONScripter::btndefCommand},
    {"btnarea", &ONScripter::btnareaCommand},
    {"btnasync", &ONScripter::btnasyncCommand},
    {"btn", &ONScripter::btnCommand},
    {"br", &ONScripter::brCommand},
    {"blt", &ONScripter::bltCommand},
    {"bgmvol", &ONScripter::mp3volCommand},
    {"bgmstop", &ONScripter::mp3stopCommand},
    {"bgmonce", &ONScripter::mp3Command},
    {"bgmfadeout", &ONScripter::mp3fadeoutCommand},
    {"bgmfadein", &ONScripter::mp3fadeinCommand},
    {"bgmdownmode", &ONScripter::bgmdownmodeCommand},
    {"bgm", &ONScripter::mp3Command},
    {"bgcpy", &ONScripter::bgcopyCommand},
    {"bgcopy", &ONScripter::bgcopyCommand},
    {"bg", &ONScripter::bgCommand},
    {"barclear", &ONScripter::barclearCommand},
    {"bar", &ONScripter::barCommand},
    {"avi", &ONScripter::aviCommand},
    {"automode_time", &ONScripter::automode_timeCommand},
    {"autoclick", &ONScripter::autoclickCommand},
    {"amsp2", &ONScripter::amspCommand},
    {"amsp", &ONScripter::amspCommand},
    {"allsp2resume", &ONScripter::allsp2resumeCommand},
    {"allspresume", &ONScripter::allspresumeCommand},
    {"allsp2hide", &ONScripter::allsp2hideCommand},
    {"allsphide", &ONScripter::allsphideCommand},
    {"abssetcursor", &ONScripter::setcursorCommand},
    {"", nullptr}};

void ONScripter::initSDL() {
	/* ---------------------------------------- */
	/* Initialize SDL */

	SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
	SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1"); // this makes no difference but better to set it

#ifdef DROID
	// For some less obvious reasons JNI setOrientation is called with 1x1 resolution on droid.
	// As a result it chooses portrait layout for us which is undesired.
	SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeRight,LandscapeLeft");
#endif

	// This is mainly needed for ANGLE when we want to use DirectX 9 on new Windows
	auto d3d = ons_cfg_options.find("d3dcompiler");
	if (d3d != ons_cfg_options.end())
		SDL_SetHint(SDL_HINT_VIDEO_WIN_D3DCOMPILER, d3d->second.c_str());

	if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0) {
		errorAndExit("Couldn't initialize SDL", SDL_GetError(), "Init Error", true);
		return; //dummy
	}

	mainThreadId = SDL_GetThreadID(nullptr);

#if defined(IOS) && defined(USE_OBJC)
	setupKeyboardHandling([](SDL_Event *k) {
		ons.localEventQueue.emplace_front(k);
	});
#endif
	SDL_StartTextInput();
#ifdef DROID
	// Prevent droid keyboard from opening!
	// Removing the start call breaks input on select devices.
	SDL_StopTextInput();
#endif

	auto cursorMode = ons_cfg_options.find("cursor");
	if (cursorMode != ons_cfg_options.end()) {
		if (cursorMode->second == "none" || cursorMode->second == "hide")
			cursorState(false);
		else if (cursorMode->second == "show")
			cursorAutoHide = false;
	}

	auto mouseScroll = ons_cfg_options.find("mouse-scrollmul");
	if (mouseScroll != ons_cfg_options.end()) {
		mouse_scroll_mul = std::stoi(mouseScroll->second);
	}

	auto touchScroll = ons_cfg_options.find("touch-scrollmul");
	if (touchScroll != ons_cfg_options.end()) {
		touch_scroll_mul = std::stoi(touchScroll->second);
	}

	auto ramLimit = ons_cfg_options.find("ramlimit");
	if (ramLimit != ons_cfg_options.end()) {
		ram_limit = std::stoi(ramLimit->second);
		if (ram_limit <= 0) {
			ons.errorAndExit("Invalid ramlimit value!");
		}
	} else {
		ram_limit = SDL_GetSystemRAM();
		if (ram_limit <= 0) {
			ram_limit = 4096;
			ons.errorAndCont("Failed to obtain ram amount, falling back to 4096 MB");
		}
	}

	size_t icon_size     = 0;
	uint8_t *icon_buffer = nullptr;
	SDL_Surface *icon    = nullptr;
	if (FileIO::readFile("icon.png", script_path, icon_size, &icon_buffer) && icon_buffer) {
		SDL_RWops *rwicon = SDL_RWFromConstMem(icon_buffer, static_cast<int>(icon_size));
		icon              = IMG_Load_RW(rwicon, 1);
		freearr(&icon_buffer);
	}

	//use icon.png preferably, but try embedded resources if not found
	//(cmd-line option --use-app-icons to prefer app resources over icon.png)
	//(Mac apps can set use-app-icons in a ons.cfg file within the
	//bundle, to have it always use the bundle icns)
	if (!icon || use_app_icons) {
#ifndef WIN32
		//backport from ponscripter
		const InternalResource *internal_icon = getResource("icon.png");
		if (internal_icon) {
			if (icon)
				SDL_FreeSurface(icon);
			SDL_RWops *rwicon = SDL_RWFromConstMem(internal_icon->buffer, static_cast<int>(internal_icon->size));
			icon              = IMG_Load_RW(rwicon, 1);
			use_app_icons     = false;
		}
#endif //WIN32
	}
	// If an icon was found (and desired), use it.
	if (icon && !use_app_icons) {
#if defined(MACOSX) || defined(WIN32)
		//resize the (usually 32x32) icon if necessary
		SDL_Surface *tmp2 = SDL_CreateRGBSurface(SDL_SWSURFACE, DEFAULT_WM_ICON_W, DEFAULT_WM_ICON_H,
		                                         32, 0x00ff0000, 0x0000ff00,
		                                         0x000000ff, 0xff000000);

		SDL_Surface *tmp = SDL_ConvertSurface(icon, tmp2->format, SDL_SWSURFACE);
		if (tmp->w == tmp2->w && tmp->h == tmp2->h) {
			//already the right size, just use converted surface as-is
			SDL_FreeSurface(tmp2);
			SDL_FreeSurface(icon);
			icon = tmp;
		} else {
			//resize converted surface
			resizeSurface(tmp, tmp2);
			SDL_FreeSurface(tmp);
			SDL_FreeSurface(icon);
			icon = tmp2;
		}
#endif //MACOSX || WIN32
	}

	script_h.setStr(&wm_title_string, DEFAULT_WM_TITLE);
	window.setTitle(wm_title_string);

	// For some systems
	if (icon && !use_app_icons)
		window.setIcon(icon);

	gpu.init();
	screen_target = gpu.rendererInit(SDL_WINDOW_ALLOW_HIGHDPI);

	auto forcedFPS = ons_cfg_options.find("force-fps");
	if (forcedFPS != ons_cfg_options.end())
		game_fps = std::stoi(forcedFPS->second);

	camera.pos.x = 0;
	camera.pos.y = 0;

	camera.center_pos = {(window.canvas_width - window.script_width) / 2.0f,
	                     (window.canvas_height - window.script_height) / 2.0f,
	                     static_cast<float>(window.script_width),
	                     static_cast<float>(window.script_height)};

	full_script_clip = {-camera.center_pos.x, -camera.center_pos.y,
	                    static_cast<float>(window.canvas_width),
	                    static_cast<float>(window.canvas_height)};

	for (auto r : {&dirty_rect_hud, &dirty_rect_scene, &before_dirty_rect_hud, &before_dirty_rect_scene}) {
		r->setDimension(SDL_Point{window.canvas_width, window.canvas_height}, camera.center_pos);
	}

	if (window.earlySetMode())
		fillCanvas();

	auto renderer = GPU_GetCurrentRenderer();
	sendToLog(LogLevel::Info, "Enabled GPU features: 0x%X\n", renderer->enabled_features);

	if (!icon || use_app_icons)
		window.setIcon();

	if (icon) {
		if (!use_app_icons) {
#ifdef MACOSX
			// There appears to be a race condition bug when relaunching on macOS.
			// Wait for a moment so that Dock gets a good icon.
			// Happens only outside of the app bundle.
			sleep(1);
#endif
			window.setIcon(icon);
		}
		//TODO: add one day?
		//cursor_gpu = gpu.copyImageFromSurface(icon);
		SDL_FreeSurface(icon);
		if (cursor_gpu)
			cursorState(false);
	}

	auto joyMapping = ons_cfg_options.find("pad-map");
	if (joyMapping != ons_cfg_options.end())
		joyCtrl.provideCustomMapping(joyMapping->second.c_str());

	joyCtrl.init();
	glyphAtlas.init();

	auto preferRumble = ons_cfg_options.find("prefer-rumble");
	if (preferRumble != ons_cfg_options.end()) {
		joyCtrl.setPreferredRumbleMethod(preferRumble->second);
	}

	gpu.pushBlendMode(BlendModeId::NORMAL);
	// gpu.clear(screen_target);
	// This appears to be particularly necessary for select droid 4.2 models leaving transparent bg.
	gpu.clearWholeTarget(screen_target, 0, 0, 0, 255);
	GPU_Flip(screen_target);

	default_audio_format.format   = AUDIO_F32;
	default_audio_format.freq     = DEFAULT_AUDIO_RATE;
	default_audio_format.channels = MIX_DEFAULT_CHANNELS;

	auto defaultFormat = ons_cfg_options.find("audioformat");
	if (defaultFormat != ons_cfg_options.end()) {
		if (defaultFormat->second == "s8")
			default_audio_format.format = AUDIO_S8;
		else if (defaultFormat->second == "u8")
			default_audio_format.format = AUDIO_U8;
		else if (defaultFormat->second == "s16")
			default_audio_format.format = AUDIO_S16;
		else if (defaultFormat->second == "u16")
			default_audio_format.format = AUDIO_U16;
		else if (defaultFormat->second == "s32")
			default_audio_format.format = AUDIO_S32;
		else if (defaultFormat->second == "f32")
			default_audio_format.format = AUDIO_F32;
	}

	openAudio(default_audio_format);
	lipsChannels.resize(ONS_MIX_CHANNELS + ONS_MIX_EXTRA_CHANNELS);

#if defined(MACOSX) && defined(USE_OBJC)
	// Remove "Preferences" (item and stripe)
	deallocateMenuItem(0, 1);
	deallocateMenuItem(0, 1);
	// Add "Mute" entry
	allocateMenuItem("Window", "Mute", {[]() {
		                 if (!ons.script_mute) {
			                 ons.volume_on_flag = !ons.volume_on_flag;
			                 ons.setVolumeMute(!ons.volume_on_flag);
			                 sendToLog(LogLevel::Info, "turned %s volume mute\n", !ons.volume_on_flag ? "on" : "off");
		                 } else {
			                 sendToLog(LogLevel::Info, "disallowed atm\n");
		                 }
	                 }});
	// We want a working fullscreen entry
	enableFullscreen([]() {
		window.changeMode(true, false, 1);
	},
	                 1);
	// We need this for error message windows
	allocateMenuEntry("Edit", 1);
	allocateMenuItem("Edit", "Select All", {"selectAll:"}, "a");
	allocateMenuItem("Edit", "Copy", {"copy:"}, "c");
#endif
}

void ONScripter::reopenAudioOnMismatch(const SDL_AudioSpec &spec) {
	//reopen the audio mixer with default settings, if needed
	if (audio_open_flag &&
	    (audio_format.format != spec.format ||
	     audio_format.channels != spec.channels ||
	     audio_format.freq != spec.freq)) {
		Mix_CloseAudio();
		openAudio(spec);
	}
}

void ONScripter::openAudio(const SDL_AudioSpec &spec) {
	auto it = ons_cfg_options.find("audiobuffer");
	if (it != ons_cfg_options.end()) {
		auto kbyte_size = std::stoi(it->second);

		if (kbyte_size > 0 && kbyte_size <= 16 && kbyte_size % 2 == 0) {
			//only allow powers of 2 as buffer sizes
			audiobuffer_size = kbyte_size * 1024;
			sendToLog(LogLevel::Info, "Using audiobuffer of %d bytes\n", audiobuffer_size);
		} else {
			std::snprintf(script_h.errbuf, MAX_ERRBUF_LEN, "Invalid audiobuffer size %dk"
			                                               " - using prior size of %d bytes",
			              kbyte_size, audiobuffer_size);
			errorAndCont(script_h.errbuf, nullptr, "Config Issue", true);
		}
	}

	it = ons_cfg_options.find("audiodriver");
	if (it != ons_cfg_options.end()) {
		SDL_setenv("SDL_AUDIODRIVER", it->second.c_str(), true);
	} else {
#ifdef WIN32
		// WASAPI driver that appeared in 2.0.7 is total garbage
		SDL_setenv("SDL_AUDIODRIVER", "directsound", false);
#endif
	}

	// Initially try to open a device not allowing anything to change.
	// Disabling frequency changes fixes the terrible click noise on Windows after SDL 2.0.7.
	// Disabling channel changes fixes signal clipping on multichannel systems, same os/SDL, thx to Simon.
	// The cause appears to be totally broken SDL audio device abilities detection, which appeared in 2.0.6.
	// Otherwise ask SDL_mixer to open a device with the default channel format (S16), and in the end
	// go with whatever the defaults are to fallback in case of issues.
	if (Mix_OpenAudioDevice(spec.freq, spec.format, spec.channels, audiobuffer_size, nullptr, 0) < 0 &&
		Mix_OpenAudioDevice(spec.freq, MIX_DEFAULT_FORMAT, spec.channels, audiobuffer_size, nullptr, 0) < 0 &&
	    Mix_OpenAudio(spec.freq, spec.format, spec.channels, audiobuffer_size) < 0) {
		errorAndCont("Couldn't open audio device!", SDL_GetError(), "Init Error", true);
		audio_open_flag = false;
	} else {
		int freq = 0, channels = 0;
		uint16_t format = 0;
		Mix_QuerySpec(&freq, &format, &channels);
		sendToLog(LogLevel::Info, "Audio: %d Hz %d bit %s %s\n",
		          freq, format & 0xFF, SDL_AUDIO_ISFLOAT(format) ? "float" : SDL_AUDIO_ISSIGNED(format) ? "sint" : "uint", channels > 1 ? "stereo" : "mono");
		audio_format.format   = format;
		audio_format.freq     = freq;
		audio_format.channels = channels;

		audio_open_flag = true;

		Mix_AllocateChannels(ONS_MIX_CHANNELS + ONS_MIX_EXTRA_CHANNELS);
		Mix_ChannelFinished(waveCallback);
	}
}

ONScripter::ONScripter()
    : ScriptParser(this),
      glyphCache(NUM_GLYPH_CACHE),
      glyphMeasureCache(NUM_GLYPH_CACHE),
      glyphAtlas(GLYPH_ATLAS_W, GLYPH_ATLAS_H) {
	//first initialize *everything* (static) to base values

	resetFlags();
	resetFlagsSub();

#ifdef PNG_AUTODETECT_NSCRIPTER_MASKS
	png_mask_type = PNG_MASK_AUTODETECT;
#elif defined PNG_FORCE_NSCRIPTER_MASKS
	png_mask_type    = PNG_MASK_USE_NSCRIPTER;
#else
	png_mask_type = PNG_MASK_USE_ALPHA;
#endif

	internal_timer = SDL_GetTicks();

	//since we've made it this far, let's init some dynamic variables
	script_h.setStr(&registry_file, REGISTRY_FILE);
	script_h.setStr(&dll_file, DLL_FILE);
	readColor(&linkcolor[0], "#FFFF22"); // yellow - link color
	readColor(&linkcolor[1], "#88FF88"); // cyan - mouseover link color
	sprite_info  = new AnimationInfo[MAX_SPRITE_NUM];
	sprite2_info = new AnimationInfo[MAX_SPRITE_NUM];

	for (int i = 0; i < MAX_SPRITE_NUM; i++) {
		sprite_info[i].id    = i;
		sprite2_info[i].id   = i;
		sprite_info[i].type  = SPRITE_LSP;
		sprite2_info[i].type = SPRITE_LSP2;
	}

	for (int i = 0; i < 3; i++) {
		tachi_info[i].id   = i;
		tachi_info[i].type = SPRITE_TACHI;
	}

	// External Players
	seqmusic_cmd = std::getenv("MUSIC_CMD");
}

void ONScripter::enableCDAudio() {
	cdaudio_flag = true;
}

void ONScripter::cursorState(bool show) {
	SDL_ShowCursor(show ? SDL_ENABLE : SDL_DISABLE);
#if defined(MACOSX) && defined(USE_OBJC)
	nativeCursorState(show);
#endif
}

void ONScripter::setMatchBgmAudio(bool flag) {
	match_bgm_audio_flag = flag;
}

void ONScripter::setRegistryFile(const char *filename) {
	script_h.setStr(&registry_file, filename);
}

void ONScripter::setDLLFile(const char *filename) {
	script_h.setStr(&dll_file, filename);
}

#ifdef WIN32
void ONScripter::setUserAppData() {
	current_user_appdata = true;
}
#endif

void ONScripter::setUseAppIcons() {
	use_app_icons = true;
}

void ONScripter::setPreferredWidth(const char *widthstr) {
	int width = static_cast<int>(std::strtol(widthstr, nullptr, 0));
	//minimum preferred window width of 160 (gets ridiculous if smaller)
	if (width > 160)
		preferred_width = width;
	else if (width > 0)
		preferred_width = 160;
}

void ONScripter::enableButtonShortCut() {
	force_button_shortcut_flag = true;
}

void ONScripter::setPreferredAutomodeTime(const char *timestr) {
	int32_t time = static_cast<int>(std::strtol(timestr, nullptr, 0));
	sendToLog(LogLevel::Info, "setting preferred automode time to %d\n", time);
	preferred_automode_time_set = true;
	automode_time = preferred_automode_time = time;
}

void ONScripter::setVoiceDelayTime(const char *timestr) {
	voicedelay_time = static_cast<int>(std::strtol(timestr, nullptr, 0));
	sendToLog(LogLevel::Info, "setting voicedelay time to %d\n", voicedelay_time);
}

void ONScripter::setVoiceWaitTime(const char *timestr) {
	voicewait_time = static_cast<int>(std::strtol(timestr, nullptr, 0));
	sendToLog(LogLevel::Info, "setting voicewait time to %d\n", voicewait_time);
}

void ONScripter::setFinalVoiceDelayTime(const char *timestr) {
	final_voicedelay_time = static_cast<int>(std::strtol(timestr, nullptr, 0));
	sendToLog(LogLevel::Info, "setting final voicedelay time to %d\n", final_voicedelay_time);
}

void ONScripter::enableWheelDownAdvance() {
	enable_wheeldown_advance_flag = true;
}

void ONScripter::setShowFPS() {
	show_fps_counter = true;
}

void ONScripter::setGameIdentifier(const char *gameid) {
	script_h.setStr(&cmdline_game_id, gameid);
}

void ONScripter::lookupSavePath() {
	const char *gameid = script_h.game_identifier.c_str();
	char gamename[20];
	if (script_h.game_identifier.empty()) {
		std::snprintf(gamename, sizeof(gamename), "ONScripter-%x", script_h.game_hash);
		gameid = gamename;
	}

	bool trycloud       = ons_cfg_options.find("disable-icloud") == ons_cfg_options.end();
	const char *storage = FileIO::getStorageDir(trycloud);

	script_h.save_path = new char[PATH_MAX];
	std::snprintf(script_h.save_path, PATH_MAX, "%s%s%c", storage, gameid, DELIMITER);

	if (!FileIO::makeDir(script_h.save_path)) {
		ons.errorAndExit("Could not create save directory!");
	}
}

int ONScripter::ownInit() {
	if (archive_path.getPathNum() == 0) {
		// Default archive_path is script directory
		DirPaths default_path(script_path);

		// Then we look in the current directory "."
		default_path.add(".");

		// And the parent directory as well:
		// #1 script parent directory
		char tmp_path[PATH_MAX];
		copystr(tmp_path, default_path.getPath(0), PATH_MAX);
		// DirPaths guarantees last character to be delimiter
		tmp_path[std::strlen(tmp_path) - 1] = '\0';
		size_t delim_pos               = FileIO::getLastDelimiter(tmp_path);
		if (delim_pos) {
			tmp_path[delim_pos] = '\0';
			if (FileIO::getLastDelimiter(tmp_path))
				default_path.add(tmp_path);
		}

		// On iOS accessing the parent folder may cause sandbox violation errors
#ifndef IOS
		// #2 current parent directory
		default_path.add("..");
#endif

		archive_path.add(default_path);
		sendToLog(LogLevel::Info, "init:archive_path: \"%s\"\n", archive_path.getAllPaths().c_str());
	}

	if (SDL_Init(0) < 0)
		ctrl.quit(-1, "Failed to load SDL internals!\nAborting everything!");

	// Prepare basic window support.
	window.init();

	if (open())
		return -1;

	if (!script_h.save_path)
		lookupSavePath();

	if (!equalstr(script_h.save_path, archive_path.getPath(0))) {
		// insert save_path onto the front of archive_path
		DirPaths new_path(script_h.save_path);
		new_path.add(archive_path);
		archive_path = new_path;
	}

	if (langdir_path[0] != '\0') {
		char path[PATH_MAX];
		std::snprintf(path, sizeof(path), "%s%s", script_path, langdir_path);

		if (!FileIO::accessFile(path, FileType::Directory))
			errorAndExit("Missing language content directory");

		archive_path.add(path);
	}

	sendToLog(LogLevel::Info, "save:archive_path: \"%s\"\n", archive_path.getAllPaths().c_str());

	auto files = FileIO::scanDir(script_path, FileType::File);

	for (auto &file : files) {
		if (script_h.isScript(file))
			script_list.emplace_back(file);
	}

#ifdef USE_LUA
	lua_handler.init(this, &script_h);
#endif

	if (debug_level > 1)
		openDebugFolders();

	initSDL();

	pixel_format_enum_32bpp = SDL_MasksToPixelFormatEnum(32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000); // RGBA
	pixel_format_enum_24bpp = SDL_MasksToPixelFormatEnum(24, 0x0000ff, 0x00ff00, 0xff0000, 0);                // RGB

	auto breakupMode = ons_cfg_options.find("breakup");
	if (breakupMode != ons_cfg_options.end()) {
		if (breakupMode->second == "old") {
			new_breakup_implementation = false;
		} else if (breakupMode->second == "newintel") {
			gpu.triangle_blit_flush = true;
		}
	}
	loadBreakupCellforms();

	auto glassSmashMode = ons_cfg_options.find("glassbreak");
	if (glassSmashMode != ons_cfg_options.end() && glassSmashMode->second == "old") {
		new_glass_smash_implementation = false;
	}

	// See: https://forum.umineko-project.org/viewtopic.php?f=2&t=273
	reduce_motion = ons_cfg_options.find("reduce-motion") != ons_cfg_options.end();

	accumulation_gpu = gpu.createImage(window.canvas_width, window.canvas_height, 4);

	// This is an accumulation surface specifically for the HUD elements.
	// It doesn't quake and only the center portion of it (script_width x script_height in size) is ever used.
	// Keeping it as a canvas_width x canvas_height sized surface rather than reducing it to script size eases use of dirty rect.
	hud_gpu = gpu.createImage(window.canvas_width, window.canvas_height, 4);

	//I wonder if it could be smaller according to setwindow4 sizes
	if (canvasTextWindow) {
		text_gpu   = gpu.createImage(window.canvas_width, window.canvas_height, 4);
		window_gpu = gpu.createImage(window.canvas_width, window.canvas_height, 4);
	} else {
		text_gpu   = gpu.createImage(window.script_width, window.script_height, 4);
		window_gpu = gpu.createImage(window.script_width, window.script_height, 4);
	}

	for (auto image : {accumulation_gpu, hud_gpu, text_gpu, window_gpu}) {
		GPU_GetTarget(image);
		gpu.clear(image->target); //Fix to avoid bg black,1 usage on bootup
	}

	// ----------------------------------------
	// Initialize misc variables
	media.init();
	auto hwdec  = ons_cfg_options.find("hwdecoder");
	auto hwconv = ons_cfg_options.find("hwconvert");
#ifdef DROID
	// At this moment hardware video decoding is slower on droid, only enable it by an explicit flag.
	bool enableHwdec = hwdec != ons_cfg_options.end() && hwdec->second == "on";
#else
	bool enableHwdec = hwdec == ons_cfg_options.end() || hwdec->second == "on";
#endif
	bool enableHwConv = hwconv == ons_cfg_options.end() || hwconv->second == "on";
	media.setHardwareDecoding(enableHwdec, enableHwConv);

	async.init();

	fonts.passReader(&script_h.reader);
	if (langdir_path[0] != '\0' && fontdir_path[0] != '\0') {
		char path[PATH_MAX];
		std::snprintf(path, sizeof(path), "%s%s", script_path, langdir_path);
		FileIO::terminatePath(path, sizeof(path));
		appendstr(path, fontdir_path, sizeof(path));
		FileIO::terminatePath(path, sizeof(path));

		if (!FileIO::accessFile(path, FileType::Directory))
			errorAndExit("Missing font content directory");

		fonts.passRoot(path);
	}
	if (fonts.init()) {
		errorAndExit("Unable to initialize Font System", nullptr, "Init Error", true);
		return -1; //dummy
	}

	// These are mostly logical dummies
	dlgCtrl.init();
	wndCtrl.init();
	dynamicProperties.init();

	pngImageLoaderPool.addLoaders(2);

	internal_timer = SDL_GetTicks();

	loadEnvData();

	defineresetCommand();
	readToken();

	return 0;
}

int ONScripter::ownDeinit() {
	reset();

	delete[] sprite_info;
	delete[] sprite2_info;

	ScriptParser::ownDeinit();

	if (screen_target)
		GPU_Quit();
	else
		SDL_Quit();

	return 0;
}

void ONScripter::reset() {
	resetFlags();

	breakupData.clear();

	script_h.setStr(&getret_str, nullptr);
	getret_int = 0;

	resetSub();

	/* ---------------------------------------- */
	/* Load global variables if available */
	if (!deinitialising()) {
		if (loadFileIOBuf("gloval.sav") == 0 ||
		    loadFileIOBuf("global.sav") == 0)
			readVariables(script_h.global_variable_border, VARIABLE_RANGE);

		loadReadLabels("readlbl.sav");
	} else {
		saveReadLabels("readlbl.sav");
	}
}

void ONScripter::resetSub() {
	int i;

	// Kill all <del>humans</del> threads

	if (initialised() && async.initialised()) {
		async.endThreads();
		async.startEventQueue();
	}

	for (i = 0; i < script_h.global_variable_border; i++)
		script_h.getVariableData(i).reset(false);

	resetFlagsSub();

	skip_mode = SKIP_NONE;
	lrTrap    = LRTrap();

	if (deinitialising())
		sentence_font_info.reset(); // Don't rebuild the sentence font info gpu image
	else
		resetSentenceFont();

	deleteNestInfo();
	deleteButtonLink();
	deleteSelectLink();

	stopCommand();
	loopbgmstopCommand();
	for (bool &ch : channel_preloaded)
		ch = false; //reset; also ensures that all dwaves stop
	stopAllDWAVE();
	script_h.setStr(&loop_bgm_name[1], nullptr);

	// ----------------------------------------
	// reset AnimationInfo
	btndef_info.reset();
	bg_info.reset();
	script_h.setStr(&bg_info.file_name, "black");
	if (!deinitialising())
		createBackground();
	for (i = 0; i < 3; i++) tachi_info[i].reset();
	for (i = 0; i < MAX_SPRITE_NUM; i++) {
		sprite_info[i].reset();
		sprite2_info[i].reset();
	}
	barclearCommand();
	prnumclearCommand();
	for (i = 0; i < 2; i++) cursor_info[i].reset();

	//reset spritesets
	resetSpritesets();

	//reset camera
	camera.resetMove();

	//Mion: reset textbtn
	deleteTextButtonInfo();
	readColor(&linkcolor[0], "#FFFF22"); // yellow - link color
	readColor(&linkcolor[1], "#88FF88"); // cyan - mouseover link color

	//reset breakup
	if (breakup_cellforms_gpu) {
		gpu.freeImage(breakup_cellforms_gpu);
		breakup_cellforms_gpu = nullptr;
	}
	if (breakup_cellform_index_grid) {
		gpu.freeImage(breakup_cellform_index_grid);
		breakup_cellform_index_grid = nullptr;
	}
	if (breakup_cellform_index_surface) {
		SDL_FreeSurface(breakup_cellform_index_surface);
		breakup_cellform_index_surface = nullptr;
	}

	if (!deinitialising())
		loadBreakupCellforms();

	// reset dialogue controller
	dlgCtrl.setDialogueActive(false);

	if (initialised() && async.initialised()) {
		// empty cache queue and cache
		// Note: No locks here because all the threads were already killed
		async.imageCacheQueue.q.clear();
		async.soundCacheQueue.q.clear();
		imageCache.clearAll();
		soundCache.clearAll();
		for (auto r : {&dirty_rect_hud, &dirty_rect_scene,
		               &before_dirty_rect_hud, &before_dirty_rect_scene}) {
			r->fill(window.canvas_width, window.canvas_height);
		}
	}
}

void ONScripter::resetFlags() {
	skip_enabled   = true;
	automode_flag  = false;
	autoclick_time = 0;
	btntime2_flag  = false;
	btntime_value  = 0;
	btnwait_time   = 0;

	is_exbtn_enabled = false;

	use_text_gradients             = false;
	use_text_gradients_for_sprites = false;
	display_mode                   = DISPLAY_MODE_NORMAL;
	event_mode                     = IDLE_EVENT_MODE;
	did_leavetext                  = false;
	skip_effect                    = false;
	effectskip_flag                = true; //on by default

	hoveredButtonNumber = -1;
	hoveringButton      = false;

	new_line_skip_flag        = false;
	draw_cursor_flag          = false;
	internal_slowdown_counter = 0;

	// ----------------------------------------
	// Sound related variables

	wave_play_loop_flag     = false;
	seqmusic_play_loop_flag = false;
	music_play_loop_flag    = false;
	cd_play_loop_flag       = false;
	mp3save_flag            = false;
	current_cd_track        = -1;

	disableGetButtonFlag();
}

void ONScripter::resetFlagsSub() {
	int i = 0;

	for (i = 0; i < 3; i++) human_order[i] = 2 - i; // "rcl"

	all_sprite_hide_flag  = false;
	all_sprite2_hide_flag = false;

	refresh_window_text_mode = REFRESH_NORMAL_MODE | REFRESH_WINDOW_MODE |
	                           REFRESH_TEXT_MODE;
	erase_text_window_mode = 1;

	monocro_flag[AfterScene] = monocro_flag[BeforeScene] = false;
	monocro_color[AfterScene] = monocro_color[BeforeScene] = {0, 0, 0, 0xFF};
	nega_mode[AfterScene] = nega_mode[BeforeScene] = 0;
	blur_mode[AfterScene] = blur_mode[BeforeScene] = 0;
	clickstr_state                                 = CLICK_NONE;

	last_keypress = SDL_NUM_SCANCODES;

	saveon_flag          = true;
	internal_saveon_flag = true;

	textgosub_clickstr_state = CLICK_NONE;
	page_enter_status        = 0;

	for (i = 0; i < SPRITE_NUM_LAST_LOADS; i++) last_loaded_sprite[i] = -1;
	last_loaded_sprite_ind = 0;

	txtbtn_start_num = next_txtbtn_num = 1;
	in_txtbtn                          = false;
	txtbtn_show                        = false;
	txtbtn_visible                     = false;
}

void ONScripter::resetSentenceFont() {
	sentence_font.reset();
	auto &style             = sentence_font.changeStyle();
	style.font_size         = DEFAULT_FONT_SIZE;
	style.wrap_limit        = 1920;
	style.character_spacing = 0;
	style.line_height       = 45;
	style.color             = {0xff, 0xff, 0xff};
	style.is_bold           = false;
	style.is_italic         = false;
	style.is_underline      = false;
	style.is_shadow         = false;
	style.is_border         = false;
	style.is_gradient       = use_text_gradients;
	style.is_centered       = false;
	style.is_fitted         = false;

	sentence_font.top_xy[0]    = 21;
	sentence_font.top_xy[1]    = 16; // + sentence_font.font_size;
	sentence_font.window_color = {0x99, 0x99, 0x99};
	sentence_font_info.reset();
	sentence_font_info.orig_pos.x = 0;
	sentence_font_info.orig_pos.y = 0;
	sentence_font_info.orig_pos.w = window.script_width;
	sentence_font_info.orig_pos.h = window.script_height;
	sentence_font_info.type       = SPRITE_SENTENCE_FONT;

	UpdateAnimPosXY(&sentence_font_info);
	UpdateAnimPosWH(&sentence_font_info);

	if (!sentence_font_info.gpu_image) {
		sentence_font_info.gpu_image = gpu.createImage(sentence_font_info.pos.w, sentence_font_info.pos.h, 4);
	}
	GPU_GetTarget(sentence_font_info.gpu_image);
	gpu.clearWholeTarget(sentence_font_info.gpu_image->target,
	                     sentence_font.window_color.x, sentence_font.window_color.y, sentence_font.window_color.z, 0xFF);
	gpu.multiplyAlpha(sentence_font_info.gpu_image);
	sentence_font_info.blending_mode = BlendModeId::MUL;

	//It is reasonably sane to reset name_font here too
	name_font.reset();
	auto &namestyle             = name_font.changeStyle();
	namestyle.font_size         = DEFAULT_FONT_SIZE;
	namestyle.wrap_limit        = 1920;
	namestyle.character_spacing = 0;
	namestyle.line_height       = 45;
	namestyle.is_bold           = false;
	namestyle.is_italic         = false;
	namestyle.is_underline      = false;
	namestyle.is_shadow         = false;
	namestyle.is_border         = false;
	namestyle.is_gradient       = use_text_gradients;
	namestyle.is_centered       = false;
	namestyle.is_fitted         = false;

	name_font.top_xy[0]           = 21;
	name_font.top_xy[1]           = 16; // + sentence_font.font_size;
	name_font.window_color        = {0x99, 0x99, 0x99};
	name_font.changeStyle().color = {0xff, 0xff, 0xff};
}

bool ONScripter::doErrorBox(const char *title, const char *errstr, bool /*is_simple*/, bool is_warning) {
	//returns true if we need to exit
	char errtitle[256];
	std::snprintf(errtitle, 256, "%s: %s", VERSION_STR1, title);

	bool ok = window.showSimpleMessageBox(is_warning ? SDL_MESSAGEBOX_WARNING : SDL_MESSAGEBOX_ERROR,
	                                      errtitle, errstr);
	if (ok && is_warning)
		return false;

	// get affairs in order
	if (errorsave) {
		saveon_flag = internal_saveon_flag = true;
		//save current game state to save999.dat,
		//without exiting if I/O Error
		saveSaveFile(999, nullptr, true);
	}

	if (debug_level > 0)
		openDebugFolders();

	return true; //should do exit
}

void ONScripter::openDebugFolders() {
	// Open the game folders in the default file manager if possible
	FileIO::shellOpen(script_path, FileType::Directory);
	FileIO::shellOpen(script_h.save_path, FileType::Directory);
}

void ONScripter::printClock(const char *str, bool print_time) {
	uint32_t this_clock_time = SDL_GetTicks();
	if (last_clock_time != 0 && print_time) {
		sendToLog(LogLevel::Info, "MS [%s]. Time since last call is %u ms (%f sec); %f secs spent in commands\n",
		          str, this_clock_time - last_clock_time,
		          (this_clock_time - last_clock_time) / 1000.0,
		          commandExecutionTime / static_cast<float>(SDL_GetPerformanceFrequency()));
	} else {
		sendToLog(LogLevel::Info, "MS [%s].\n", str);
	}
	last_clock_time = this_clock_time;
}

void ONScripter::flush(int refresh_mode, GPU_Rect *scene_rect, GPU_Rect *hud_rect, bool clear_dirty_flag, bool direct_flag, bool wait_for_cr) {

	if (!(refresh_mode & CONSTANT_REFRESH_MODE)) {
		refresh_mode &= ~REFRESH_BEFORESCENE_MODE;
		constant_refresh_mode |= refresh_mode;
		if (wait_for_cr) {
			constant_refresh_executed = false;
			// Process events until constant refresh has executed, then return
			event_mode = IDLE_EVENT_MODE;
			while (!constant_refresh_executed) {
				//int old_event_mode = event_mode; //saving is probably unnecessary....
				waitEvent(0);
				//event_mode = old_event_mode;
			}
		}
		return; // Ignore this flush. Do not rebuild the scene or erase any dirty rects.
	}

	if (direct_flag || pre_screen_render || onionAlphaCooldown || onionAlphaFactor) {
		GPU_Rect full_rect = full_script_clip;
		if (!scene_rect)
			scene_rect = &full_rect;
		if (!hud_rect)
			hud_rect = &full_rect;
		flushDirect(*scene_rect, *hud_rect, refresh_mode);
		scene_rect = hud_rect = nullptr;
	} else {
		if (refresh_mode & REFRESH_BEFORESCENE_MODE) {
			if (scene_rect)
				before_dirty_rect_scene.add(*scene_rect);
			if (hud_rect)
				before_dirty_rect_hud.add(*hud_rect);

			if ((!before_dirty_rect_scene.isEmpty() || !before_dirty_rect_hud.isEmpty()) || camera.has_moved) {
				flushDirect(before_dirty_rect_scene.bounding_box_script, before_dirty_rect_hud.bounding_box_script, refresh_mode);
			}
		} else {
			if (scene_rect)
				dirty_rect_scene.add(*scene_rect);
			if (hud_rect)
				dirty_rect_hud.add(*hud_rect);

			if ((!dirty_rect_scene.isEmpty() || !dirty_rect_hud.isEmpty()) || camera.has_moved) {
				flushDirect(dirty_rect_scene.bounding_box_script, dirty_rect_hud.bounding_box_script, refresh_mode);
			}
		}
	}

	if (clear_dirty_flag && pre_screen_render) {
		before_dirty_rect_scene.clear();
		before_dirty_rect_hud.clear();
		dirty_rect_scene.clear();
		dirty_rect_hud.clear();
	} else if (clear_dirty_flag && !(refresh_mode & REFRESH_BEFORESCENE_MODE)) {
		dirty_rect_scene.clear();
		dirty_rect_hud.clear();
	} else if (clear_dirty_flag) {
		before_dirty_rect_scene.clear();
		before_dirty_rect_hud.clear();
	}

	//sendToLog(LogLevel::Info, "exited flush.\n");
}

void ONScripter::createScreenshot(GPU_Image *first, GPU_Rect *first_r, GPU_Image *second, GPU_Rect *second_r) {
	char filename[64];
	std::snprintf(filename, sizeof(filename), "%ld.png", time(nullptr));
	FILE *fp = FileIO::openFile(filename, "wb", script_h.save_path);
	if (fp) {
		auto screenshot = gpu.getScriptImage();
		GPU_GetTarget(screenshot);
		GPU_SetBlending(first, false);
		gpu.copyGPUImage(first, first_r, nullptr, screenshot->target, first_r ? 0 : -(first->w - screenshot->w) / 2.0, first_r ? 0 : -(first->h - screenshot->h) / 2.0);
		GPU_SetBlending(first, true);
		if (second)
			gpu.copyGPUImage(second, second_r, nullptr, screenshot->target, second_r ? 0 : -(second->w - screenshot->w) / 2.0, second_r ? 0 : -(second->h - screenshot->h) / 2.0);
		auto rwops = SDL_RWFromFP(fp, SDL_TRUE);
		GPU_SaveImage_RW(screenshot, rwops, true, GPU_FILE_PNG);
		gpu.giveScriptImage(screenshot);
		needs_screenshot = false;
	} else {
		sendToLog(LogLevel::Error, "Failed to save screenshot (%s)\n", filename);
	}
}

void ONScripter::flushDirect(GPU_Rect &scene_rect, GPU_Rect &hud_rect, int refresh_mode) {
	if (!(refresh_mode & CONSTANT_REFRESH_MODE)) {
		refresh_mode &= ~REFRESH_BEFORESCENE_MODE;
		constant_refresh_mode |= refresh_mode;
		return; // Ignore this flush. Do not rebuild the scene.
	}

	if (!(skip_mode & SKIP_SUPERSKIP)) {
		bool startingOnionAlpha{false};
		if (!onionAlphaCooldown && onionAlphaFactor) {
			// if we're just beginning onion alpha, then work up from 0 so we don't get a weird flicker due to not having a solid background
			startingOnionAlpha = true;
		}

		if (onionAlphaFactor > onionAlphaCooldown) {
			onionAlphaCooldown = onionAlphaFactor;
		}

		// Should always be true only for startingOnionAlpha, but just in case for safety reasons.
		if (onion_alpha_gpu == nullptr && (startingOnionAlpha || onionAlphaCooldown))
			onion_alpha_gpu = gpu.getScriptImage();
		if (!pre_screen_gpu)
			pre_screen_gpu = gpu.getScriptImage();

		if (onionAlphaCooldown || startingOnionAlpha) {
			if (!pre_screen_render)
				combineWithCamera(accumulation_gpu, hud_gpu, pre_screen_gpu->target, scene_rect, hud_rect, refresh_mode);
		} else {
			if (pre_screen_render) {
				if (needs_screenshot)
					createScreenshot(pre_screen_gpu, nullptr);
				gpu.copyGPUImage(pre_screen_gpu, nullptr, nullptr, screen_target);
			} else {
				combineWithCamera(accumulation_gpu, hud_gpu, screen_target, scene_rect, hud_rect, refresh_mode);
			}
		}

		if (startingOnionAlpha) {
			gpu.copyGPUImage(pre_screen_gpu, nullptr, nullptr, onion_alpha_gpu->target);
			if (needs_screenshot)
				createScreenshot(onion_alpha_gpu, nullptr);
			gpu.copyGPUImage(onion_alpha_gpu, nullptr, nullptr, screen_target);
		} else if (onionAlphaCooldown && onion_alpha_gpu) {
			GPU_SetRGBA(pre_screen_gpu, 255 - onionAlphaCooldown, 255 - onionAlphaCooldown, 255 - onionAlphaCooldown, 255 - onionAlphaCooldown);
			auto scalef = onionAlphaScale / 1000.0;
			gpu.copyGPUImage(pre_screen_gpu, nullptr, nullptr, onion_alpha_gpu->target, 0, 0, scalef, scalef);
			GPU_SetRGBA(pre_screen_gpu, 255, 255, 255, 255);
			if (needs_screenshot)
				createScreenshot(onion_alpha_gpu, nullptr);
			gpu.copyGPUImage(onion_alpha_gpu, nullptr, nullptr, screen_target);
			onionAlphaCooldown *= (255.0 - onionAlphaCooldown) / 255.0;
			if (onionAlphaCooldown <= 10) {
				onionAlphaCooldown = 0;
				gpu.giveScriptImage(onion_alpha_gpu);
				onion_alpha_gpu = nullptr;
			}
		}
	}

	camera.has_moved = false;
	screenChanged    = true;
}

void ONScripter::combineWithCamera(GPU_Image *scene, GPU_Image *hud, GPU_Target *dst, GPU_Rect &scene_rect, GPU_Rect &hud_rect, int refresh_mode) {
	//sendToLog(LogLevel::Info, "combineWithCamera called rm %d\n", refresh_mode);

	if (scene != nullptr && hud != nullptr) {
		refreshSceneTo(scene->target, &scene_rect, refresh_mode);
		refreshHudTo(hud->target, &hud_rect, refresh_mode);
	} else {
		sendToLog(LogLevel::Error, "Null accumulation surface AT LEAST\n");
	}
	GPU_Rect combined_camera = camera.center_pos;
	combined_camera.x -= camera.pos.x;
	combined_camera.y -= camera.pos.y;
	GPU_SetBlending(scene, false); // in breakup transition, the (broken-up) scene may have holes! make sure we blank those areas in dst too
	gpu.copyGPUImage(scene, &combined_camera, nullptr, dst);
	GPU_SetBlending(scene, true);
	gpu.copyGPUImage(hud, &camera.center_pos, nullptr, dst);

	if (needs_screenshot)
		createScreenshot(scene, &combined_camera, hud, &camera.center_pos);
}

void ONScripter::mouseOverCheck(int x, int y, bool forced) {
	// making it return if unchanged might break things xD will take it easy and just use a bool for now
	bool mouseChanged = (last_mouse_state.x != x || last_mouse_state.y != y);

	last_mouse_state.x = x;
	last_mouse_state.y = y;

	/* ---------------------------------------- */
	/* Check scrollables */
	if (mouseChanged || forced) {
		for (int i = 0; i < MAX_SPRITE_NUM; i++) {
			AnimationInfo &ai = sprite_info[i];
			if (ai.scrollableInfo.isSpecialScrollable && ai.scrollableInfo.respondsToMouseOver) {
				ai.scrollableInfo.mouseCursorIsOverHoveredElement = false;
				if (x >= ai.pos.x && y >= ai.pos.y + ai.scrollableInfo.firstMargin && x <= ai.pos.x + ai.pos.w && y <= ai.pos.y + ai.pos.h - ai.scrollableInfo.lastMargin)
					mouseOverSpecialScrollable(i, x - ai.pos.x, y + ai.scrollable.y - ai.pos.y);
			}
		}
	}

	/* ---------------------------------------- */
	/* Check button */
	bool found{false};
	int buttonNumber{0};
	ButtonLink *buttonLink = root_button_link.next;
	int buttonLinkIndex    = -1;

	ButtonLink *cur_button_link = nullptr;
	while (buttonLink) {
		buttonLinkIndex++;
		cur_button_link = buttonLink;
		while (cur_button_link) {
			if (x >= cur_button_link->select_rect.x &&
			    x < cur_button_link->select_rect.x + cur_button_link->select_rect.w &&
			    y >= cur_button_link->select_rect.y &&
			    y < cur_button_link->select_rect.y + cur_button_link->select_rect.h &&
			    (cur_button_link->button_type != ButtonLink::TEXT_BUTTON ||
			     (txtbtn_visible && txtbtn_show))) {
				bool in_button = true;
				if (transbtn_flag) {
					AnimationInfo *anim = nullptr;
					int alpha = 0;
					in_button = false;
					if (cur_button_link->button_type == ButtonLink::SPRITE_BUTTON ||
						cur_button_link->button_type == ButtonLink::EX_SPRITE_BUTTON) {
						anim = &sprite_info[cur_button_link->sprite_no];
						alpha = anim->getPixelAlpha(x - cur_button_link->select_rect.x,
													y - cur_button_link->select_rect.y);
					} else if (cur_button_link->anim) {
						alpha = cur_button_link->anim->getPixelAlpha(x - cur_button_link->select_rect.x,
																	 y - cur_button_link->select_rect.y);
					}
					if (alpha > TRANSBTN_CUTOFF)
						in_button = true;
				}
				if (in_button) {
					buttonNumber = cur_button_link->no;
					found        = true;
					break;
				}
			}
			cur_button_link = cur_button_link->same;
		}
		if (found)
			break;
		buttonLink = buttonLink->next;
	}

	doHoverButton(found, buttonNumber, buttonLinkIndex, buttonLink);
}

void ONScripter::doHoverButton(bool hovering, int buttonNumber, int buttonLinkIndex, ONScripter::ButtonLink *buttonLink) {
	if (hovering && hoveringButton && hoveredButtonNumber == buttonNumber) {
		return;
	}
	if (!hovering && !hoveringButton && hoveredButtonNumber == -1) {
		return;
	}

	//Normally, all buttons should be placed to hud but nobody disallows doing the opposite
	//Make sure we update every button we need
	GPU_Rect check_src_rect{0, 0, 0, 0};
	GPU_Rect check_dst_rect{0, 0, 0, 0};
	updateButtonsToDefaultState(check_src_rect, check_dst_rect);

	if (hovering) {
		// FIXME: we cannot call playSoundThreaded from inside doHoverButton as it may be inside waitEvent.
		if (selectvoice_file_name[SELECTVOICE_OVER]) {
			errorAndExit("Can't call playSoundThreaded from inside doHoverButton. selectvoice unsupported here.");
			playSoundThreaded(selectvoice_file_name[SELECTVOICE_OVER],
			                  SOUND_CHUNK, false, MIX_WAVE_CHANNEL);
		}
		ButtonLink *cur_button_link = buttonLink;
		while (cur_button_link) {
			check_dst_rect = cur_button_link->image_rect;
			if (cur_button_link->button_type == ButtonLink::SPRITE_BUTTON ||
			    cur_button_link->button_type == ButtonLink::EX_SPRITE_BUTTON) {
				auto anim = &sprite_info[cur_button_link->sprite_no];
				anim->setCell(1);
				anim->visible = true;
				if ((cur_button_link == buttonLink) && is_exbtn_enabled &&
				    (cur_button_link->button_type == ButtonLink::EX_SPRITE_BUTTON)) {
					decodeExbtnControl(cur_button_link->exbtn_ctl, &check_src_rect, &check_dst_rect);
				}
			} else if (cur_button_link->button_type == ButtonLink::TMP_SPRITE_BUTTON) {
				cur_button_link->show_flag = true;
				auto anim                  = cur_button_link->anim;
				if (anim) {
					anim->setCell(1);
					anim->visible = true;
				}
			} else if (cur_button_link->button_type == ButtonLink::TEXT_BUTTON &&
			           txtbtn_show && txtbtn_visible) {
				cur_button_link->show_flag = true;
				auto anim                  = cur_button_link->anim;
				if (anim) {
					anim->setCell(1);
					anim->visible = true;
				}
				if ((cur_button_link == buttonLink) &&
				    is_exbtn_enabled && cur_button_link->exbtn_ctl) {
					decodeExbtnControl(cur_button_link->exbtn_ctl, &check_src_rect, &check_dst_rect);
				}
			} else if (cur_button_link->button_type == ButtonLink::NORMAL_BUTTON ||
			           cur_button_link->button_type == ButtonLink::LOOKBACK_BUTTON) {
				cur_button_link->show_flag = true;
			}
			dirtyRectForZLevel(cur_button_link->sprite_no, cur_button_link->image_rect);
			cur_button_link = cur_button_link->same;
		}
		lastKnownHoveredButtonLinkIndex = buttonLinkIndex;
		lastKnownHoveredButtonNumber    = buttonNumber;
	}
	hoveringButton              = hovering;
	hoveredButtonNumber         = hovering ? buttonNumber : -1;
	previouslyHoveredButtonLink = hovering ? buttonLink : nullptr;
	//sendToLog(LogLevel::Info, "flush from doHoverButton. hoveredButtonNumber=%i.\n", hoveredButtonNumber);
	flush(refreshMode());
}

void ONScripter::updateButtonsToDefaultState(GPU_Rect &check_src_rect, GPU_Rect &check_dst_rect) {
	if (previouslyHoveredButtonLink == nullptr) {
		// Nothing was previously hovered, so just set all buttons to defaults.
		if (is_exbtn_enabled && exbtn_d_button_link.exbtn_ctl) {
			decodeExbtnControl(exbtn_d_button_link.exbtn_ctl, &check_src_rect, &check_dst_rect);
		}
	} else {
		// Something was previously hovered, so deselect it and dirty its sprite rect before setting all buttons to defaults.
		ButtonLink *cur_button_link = previouslyHoveredButtonLink;
		while (cur_button_link) {
			cur_button_link->show_flag = false;
			check_src_rect             = cur_button_link->image_rect;
			if (cur_button_link->button_type == ButtonLink::SPRITE_BUTTON ||
			    cur_button_link->button_type == ButtonLink::EX_SPRITE_BUTTON) {
				sprite_info[cur_button_link->sprite_no].visible = true;
				sprite_info[cur_button_link->sprite_no].setCell(0);
			} else if (cur_button_link->button_type == ButtonLink::TMP_SPRITE_BUTTON) {
				cur_button_link->show_flag     = true;
				cur_button_link->anim->visible = true;
				cur_button_link->anim->setCell(0);
			} else if (cur_button_link->button_type == ButtonLink::TEXT_BUTTON) {
				if (txtbtn_visible) {
					cur_button_link->show_flag     = true;
					cur_button_link->anim->visible = true;
					cur_button_link->anim->setCell(0);
				}
			}
			dirtyRectForZLevel(cur_button_link->sprite_no, cur_button_link->image_rect);
			if (is_exbtn_enabled && exbtn_d_button_link.exbtn_ctl) {
				// Why is this inside the loop? Won't it do something insane like play sounds twice if "same" is used and "S" command is used in exbtn?
				decodeExbtnControl(exbtn_d_button_link.exbtn_ctl, &check_src_rect, &check_dst_rect);
			}

			cur_button_link = cur_button_link->same;
		}
	}
}

void ONScripter::executeLabel() {
	int last_token_line = -1;

	while (true) {
		while (current_line < current_label_info->num_of_lines) {
			if ((debug_level > 1) && (last_token_line != current_line) &&
			    (script_h.getStringBufferR()[0] != 0x0a)) {
				sendToLog(LogLevel::Info, "\n*****  executeLabel %s:%d/%d:mode=%s *****\n",
				          current_label_info->name,
				          current_line,
				          current_label_info->num_of_lines,
				          (display_mode == 0 ? "normal" : (display_mode == 1 ? "text" : "updated")));
				fflush(stdout);
			}
			last_token_line = current_line;

			if (script_h.getStringBufferR()[0] == '~') {
				last_tilde.next_script = script_h.getNext();
				readToken();
				continue;
			}
			if (break_flag && !script_h.isName("next", true)) {
				if (script_h.getStringBufferR()[0] == 0x0a)
					current_line++;

				if ((script_h.getStringBufferR()[0] != ':') &&
				    (script_h.getStringBufferR()[0] != ';') &&
				    (script_h.getStringBufferR()[0] != 0x0a))
					script_h.skipToken();

				readToken();
				continue;
			}

			if (kidokuskip_flag && (skip_mode & SKIP_NORMAL) &&
			    kidokumode_flag && !script_h.isKidoku())
				skip_mode &= ~SKIP_NORMAL;

			static unsigned int waitEventCounter{0};
			if (!atomic_flag) {
				waitEventCounter = (waitEventCounter + 1) % 5000; // run once every 5000 commands in superskip mode
				if (!(skip_mode & SKIP_SUPERSKIP) || !waitEventCounter) {
					bool waitedOnce = false;
					while (takeEventsOut(ONS_UPKEEP_EVENT)) {
						waitedOnce = true;
						waitEvent(0);
					}
					if (!waitedOnce) {
						// We assume that no command wants to share its event with the other command, unless it uses a CRAction
						// Reset event_mode to idle, to avoid any collisions of not freed event_mode (e.g. in clickCommand)
						event_mode = IDLE_EVENT_MODE;
						//We must return control to waitEvent here to give the screen a chance to update when it is time
						//  (Otherwise long script for-loops etc may never give script a chance to waitEvent and hence refresh the screen)
						//We'll pass nopPreferred so that we come back here immediately unless we really do need to draw
						waitEvent(0, true);
					}
				}
			}

			// This could be in event loop, but it's just a few boolean checks and honestly event loop is too bloated already
			// (Plus having it in here will ensure it always responds at the very moment it is necessary, and besides, it does not actually interface with any particular event from SDL)
			// (Shouldn't there be some kind of other function than the event loop for checks just like these? I can't believe this is the first instance)
			if (!(skip_mode & SKIP_SUPERSKIP)) {
				if (!skipIsAllowed() && (keyState.ctrl || skip_mode)) {
					keyState.ctrl         = 0;
					skip_mode             = 0;
					eventCallbackRequired = true;
				}
			}

			int ret{RET_NO_READ};
			if (event_callback_label && eventCallbackRequired && !inVariableQueueSubroutine && !callStackHasUninterruptible) {
				gosubReal(event_callback_label, script_h.getCurrent());
				eventCallbackRequired = false;
				ret                   = RET_CONTINUE;
			} else if (dlgCtrl.wantsControl() && !callStackHasUninterruptible) {
				ret = dlgCtrl.processDialogueEvents();
			} else if (scriptExecutionPermitted()) {

				//static auto prevEnd = SDL_GetPerformanceCounter();
				//auto start = SDL_GetPerformanceCounter();

				// Very useful debugging code! :)
				// Uncomment to use
				/*{
					std::ostringstream logStream;
					logStream << "Since last command: " << (start-prevEnd);
					if (script_h.debugCommandLog.size() > 300) script_h.debugCommandLog.pop_front();
					script_h.debugCommandLog.push_back(logStream.str());
				}
				
				{
					auto st = script_h.getCurrent();
					auto firstRN0 = strpbrk(st, "\r\n\0");
					int eol = firstRN0 ? firstRN0 - st : 0;
					std::string log;
					log.insert(0, st, eol);
					//if (script_h.getStringBuffer()) {
					//	log += "(((";
					//	log += script_h.getStringBuffer();
					//	log += ")))";
					//}
					if (script_h.debugCommandLog.size() > 300) script_h.debugCommandLog.pop_front();
					script_h.debugCommandLog.push_back(log);
					log.clear();
				}*/

				// count script execution time
				auto start = SDL_GetPerformanceCounter();
				ret        = ScriptParser::parseLine();
				if (ret == RET_NOMATCH)
					ret = this->parseLine();
				commandExecutionTime += SDL_GetPerformanceCounter() - start;

				/*std::ostringstream logStream;
				logStream << "Command execution time: " << (end-start);
				if (script_h.debugCommandLog.size() > 300) script_h.debugCommandLog.pop_front();
				script_h.debugCommandLog.push_back(logStream.str());
				
				prevEnd = end;*/
			}

			// These need to execute in both cases.
			if (ret & (RET_SKIP_LINE | RET_EOL)) {
				if (ret & RET_SKIP_LINE)
					script_h.skipLine();
				if (++current_line >= current_label_info->num_of_lines)
					break;
			}

			if (!(ret & RET_NO_READ))
				readToken();
		}

		current_label_info = script_h.lookupLabelNext(current_label_info->name);
		current_line       = 0;
		last_token_line    = -1;

		if (current_label_info->start_address != nullptr) {
			if (!tryEndSuperSkip(false))
				script_h.setCurrent(current_label_info->label_header);
			readToken();
			continue;
		}

		break;
	}

	sendToLog(LogLevel::Info, " ***** End *****\n");
	endCommand();
}

bool ONScripter::tryEndSuperSkip(bool force) {
	if (!force && (!(skip_mode & SKIP_SUPERSKIP)
		|| superSkipData.dst_lbl.empty()
		|| !equalstr(superSkipData.dst_lbl.c_str() + 1, current_label_info->name)
		|| script_h.choiceState.acceptChoiceNextIndex !=
		    static_cast<uint32_t>(script_h.choiceState.acceptChoiceVectorSize))) {
		return false;
	}

	if (superSkipData.dst_lbl.empty()) {
		errorAndExit("Tried to end super skip with an empty dst_lbl -- maybe it wasn't even active?");
	}
	std::string endLabel = "*";
	endLabel += current_label_info->name;
	script_h.setStr(&script_h.getVariableData(superSkipData.dst_var).str, endLabel.c_str());
	// so... need to restore caller state...
	script_h.swapScriptStateData(superSkipData.callerState);
	// and then throw away the superskip data?
	superSkipData = SuperSkipData();
	script_h.choiceState.acceptChoiceVectorSize = -1;
	// Your script function is now required to call sskip_unset to signal to ons that it is OK to update the screen.

	return true;
}

bool ONScripter::scriptExecutionPermitted() {
	if (callStackHasUninterruptible)
		return true;

	bool dlgReady = dlgCtrl.dialogueProcessingState.readyToRun;

	/* Dialogues work very differently to how you might expect.
	 * The dialogues and the main script proceed in "parallel" in an interleaved fashion.
	 * "Loan execution" returns control from inline dialogue commands back to the main script.
	 * This can be used to suspend inline dialogue commands from executing (e.g. to implement wait commands)
	 * in a way that allows the main script to continue, even while the dialogue is only halfway executed.
	 * The main script can also issue d_continue to pass control back to the dialogue again. That will cause
	 * the dialogue to wantsControl() and endLoanExecution(), returning execution to the inline dialogue commands.
	 *
	 * To repeat, loaning execution to main script prevents the execution of inline dialogue commands.
	 *
	 * We prevent script from executing regular commands (not ones that originate from dialogue-inline) if:*/
	if (dlgCtrl.loanExecutionActive || !dlgCtrl.executingDialogueInlineCommand) {
		// 1: we are executing d (and not d2). This prevents commands following a non-piped dialogue from being executed -- everything happens before moving on to the next line.
		if (dlgReady && !dlgCtrl.continueScriptExecution)
			return false;

		// 2: the script is suspended by a wait_on_d in the script (thooooough... this could feasibly be an action as well...)
		for (auto &pair : dlgCtrl.suspendScriptPasses) {
			if (dlgReady && pair.second <= -1)
				return false;
		}

		// 3: the script is suspended by a currently executing action
		for (const auto &a : getConstantRefreshActions()) {
			if (a->suspendsMainScript())
				return false;
		}
	}

	return true;
}

void ONScripter::runScript() {
	readToken();

	int ret = ScriptParser::parseLine();
	if (ret == RET_NOMATCH)
		this->parseLine();
}

bool ONScripter::isBuiltInCommand(const char *cmd) {
	return ScriptParser::isBuiltInCommand(cmd) || func_lut.count(cmd[0] == '_' ? cmd + 1 : cmd);
}

int ONScripter::evaluateBuiltInCommand(const char *cmd) {
	// Execute a builtin command only if it is present
	auto ret = ScriptParser::evaluateCommand(cmd, true, false, true);
	if (ret != RET_NOMATCH)
		return ret;

	auto it = func_lut.find(cmd);
	if (it != func_lut.end())
		return (this->*it->second)();

	char error[4096];
	std::snprintf(error, 4096, "Failed to evaluate a system command: %s", cmd);
	errorAndExit(error);

	return RET_CONTINUE;
}

int ONScripter::parseLine() {
	std::string &cmd = script_h.getStringBufferRW();
	if (cmd[0] == '_')
		cmd.erase(0, 1);
	const char *s_buf = script_h.current_cmd;

	struct ProfileData {
		Uint64 time{0};
		Uint64 runs{0};
	};
	static std::unordered_map<std::string, ProfileData> profileData;
	static bool profiling{false};

	if (profiling && !(skip_mode & SKIP_SUPERSKIP)) {
		profiling = false;
		sendToLog(LogLevel::Warn, "Function name,Time,Runs\n");
		for (auto &pair : profileData) {
			const std::string &fn = pair.first;
			ProfileData &pd       = pair.second;
			sendToLog(LogLevel::Warn, "%s,%llu,%llu\n", fn.c_str(), pd.time, pd.runs);
		}
		profileData.clear();
	}
	if (!profiling && (skip_mode & SKIP_SUPERSKIP)) {
		// Set to true to enable superskip profiling.
		profiling = false;
	}

	//Check against builtin cmds
	auto it = func_lut.find(s_buf);
	if (it != func_lut.end()) {
		uint64_t start{0};
		if (profiling) {
			start = SDL_GetPerformanceCounter();
		}
		int r = (this->*it->second)();
		if (profiling) {
			auto time       = SDL_GetPerformanceCounter() - start;
			ProfileData &fn = profileData[s_buf];
			fn.time += time;
			fn.runs++;
		}
		return r;
	}

	script_h.current_cmd_type = ScriptHandler::CmdType::BuiltIn;
	if (*s_buf == 0x0a) {
		script_h.current_cmd_type = ScriptHandler::CmdType::None;
		return RET_CONTINUE | RET_EOL;
	}
	if ((s_buf[0] == 'v') && (s_buf[1] >= '0') && (s_buf[1] <= '9')) {
		copystr(script_h.current_cmd, "vNUM", sizeof(script_h.current_cmd));
		return vCommand();
	}
	if ((s_buf[0] == 'd') && (s_buf[1] == 'v') &&
	    (s_buf[2] >= '0') && (s_buf[2] <= '9')) {
		copystr(script_h.current_cmd, "dvNUM", sizeof(script_h.current_cmd));
		return dvCommand();
	}
	if ((s_buf[0] == 'm') && (s_buf[1] == 'v') &&
	    (s_buf[2] >= '0') && (s_buf[2] <= '9')) {
		copystr(script_h.current_cmd, "mvNUM", sizeof(script_h.current_cmd));
		return mvCommand();
	}

	script_h.current_cmd_type = ScriptHandler::CmdType::Unknown;
	std::snprintf(script_h.errbuf, MAX_ERRBUF_LEN, "command [%s] is not supported yet!!", s_buf);
	errorAndCont(script_h.errbuf);

	script_h.skipToken();

	return RET_CONTINUE;
}

void ONScripter::readToken() {
	bool pretext_check = false;
	if (pretextgosub_label &&
	    (!pagetag_flag || (page_enter_status == 0)))
		pretext_check = true;

	script_h.readToken(pretext_check);
	string_buffer_offset = 0;
}

/* ---------------------------------------- */
void ONScripter::processTextButtonInfo() {
	TextButtonInfoLink *info = text_button_info.next;

	if (info)
		txtbtn_show = true;
	while (info) {
		ButtonLink *firstbtn = nullptr;
		char *text           = info->prtext;
		char *text2;
		Fontinfo f_info = sentence_font;
		//f_info.clear();
		f_info.off_color = linkcolor[0];
		f_info.on_color  = linkcolor[1];
		do {
			text2 = std::strchr(text, 0x0a);
			if (text2) {
				*text2 = '\0';
			}
			ButtonLink *txtbtn = getSelectableSentence(text, &f_info, true, false, false);
			//sendToLog(LogLevel::Info, "made txtbtn: %d '%s'\n", info->no, text);
			txtbtn->button_type = ButtonLink::TEXT_BUTTON;
			txtbtn->no          = info->no;
			if (!txtbtn_visible)
				txtbtn->show_flag = false;
			if (firstbtn)
				firstbtn->connect(txtbtn);
			else
				firstbtn = txtbtn;
			f_info.newLine();
			if (text2) {
				*text2 = 0x0a;
				text2++;
			}
			text = text2;
		} while (text2);
		root_button_link.insert(firstbtn);
		info->button = firstbtn;
		info         = info->next;
	}
}

void ONScripter::deleteTextButtonInfo() {
	TextButtonInfoLink *i1 = text_button_info.next;

	while (i1) {
		TextButtonInfoLink *i2 = i1;
		// need to hide textbtn links
		ButtonLink *cur_button_link = i2->button;
		while (cur_button_link) {
			cur_button_link->show_flag = false;
			cur_button_link            = cur_button_link->same;
		}
		i1 = i1->next;
		delete i2;
	}
	text_button_info.next = nullptr;
	txtbtn_visible        = false;
	next_txtbtn_num       = txtbtn_start_num;
}

void ONScripter::deleteButtonLink() {
	ButtonLink *b1 = root_button_link.next;

	while (b1) {
		ButtonLink *b2 = b1->same;
		while (b2) {
			ButtonLink *b3 = b2;
			b2             = b2->same;
			delete b3;
		}
		b2 = b1;
		b1 = b1->next;
		if (b2->button_type == ButtonLink::TEXT_BUTTON) {
			// Need to delete ref to button from text_button_info
			TextButtonInfoLink *i1 = text_button_info.next;
			while (i1) {
				if (i1->button == b2)
					i1->button = nullptr;
				i1 = i1->next;
			}
		}
		delete b2;
	}
	root_button_link.next       = nullptr;
	previouslyHoveredButtonLink = nullptr;
	hoveredButtonNumber         = -1;
	hoveringButton              = false;
	hoveredButtonDefaultNumber  = -1;

	freearr(&exbtn_d_button_link.exbtn_ctl);
	is_exbtn_enabled = false;
}

void ONScripter::refreshButtonHoverState(bool forced) {
	if (controlMode == ControlMode::Mouse) {
		// Without these, buttons fail to highlight correctly when in a btnwait loop that doesn't call btndef repeatedly.
		// These sets might want to go into both cases. Not sure.
		hoveringButton      = false;
		hoveredButtonNumber = -1;
		// Design decision:
		// Could set lastKnownHoveredButton fields to -1 here.
		// Without setting to -1:
		//    After a mouse movement that doesn't hover a button, arrow keys will start the cursor at the last place you did hover over, rehighlighting it.
		// With setting to -1:
		//    After a mouse movement that doesn't hover a button, arrow keys will start the cursor at the default position for that menu.
		int mx, my;
		SDL_GetMouseState(&mx, &my);
		window.translateWindowToScriptCoords(mx, my);
		mouseOverCheck(mx, my, forced);
	} else {
		shiftHoveredButtonInDirection(0);
	}
}

/* ---------------------------------------- */
/* Delete select link */
void ONScripter::deleteSelectLink() {
	SelectLink *link, *last_select_link = root_select_link.next;

	while (last_select_link) {
		link             = last_select_link;
		last_select_link = last_select_link->next;
		delete link;
	}
	root_select_link.next = nullptr;
}

void ONScripter::clearCurrentPage() {
	gpu.clearWholeTarget(text_gpu->target);
	if (wndCtrl.usingDynamicTextWindow)
		gpu.clearWholeTarget(window_gpu->target);
	sentence_font.clear();

	internal_saveon_flag = true;

	deleteTextButtonInfo();
}

void ONScripter::newPage(bool next_flag, bool no_flush) {
	if (next_flag) {
		page_enter_status = 0;
	}

	clearCurrentPage();
	txtbtn_visible = false;
	txtbtn_show    = false;

	if (!no_flush) {
		//According to ONScripter-EN sources and docs, texec does a refresh and modifies the screen. This means, we should commit to do this.
		//Hope it won't make us deal with other bugs
		commitVisualState();
		// Previously we were passing a window rect as a hud_rect param of flush, which was not making any sense
		addTextWindowClip(before_dirty_rect_hud);
		addTextWindowClip(dirty_rect_hud);
		flush(refreshMode(), nullptr, nullptr, true, false, true);
	}
}

AnimationInfo *ONScripter::getSentence(char *buffer, Fontinfo *info, int num_cells, bool /*flush_flag*/, bool nofile_flag, bool skip_whitespace) {
	//Mion: moved from getSelectableSentence and modified
	AnimationInfo *anim = new AnimationInfo();

	anim->trans_mode   = AnimationInfo::TRANS_STRING;
	anim->num_of_cells = num_cells;
	anim->color_list   = new uchar3[num_cells];
	if (nofile_flag)
		anim->color_list[0] = info->nofile_color;
	else
		anim->color_list[0] = info->off_color;
	if (num_cells > 1)
		anim->color_list[1] = info->on_color;
	anim->skip_whitespace = skip_whitespace;
	script_h.setStr(&anim->file_name, buffer);
	anim->orig_pos.x = info->x();
	anim->orig_pos.y = info->y();
	UpdateAnimPosXY(anim);
	anim->visible = true;

	setupAnimationInfo(anim, info);

	info->newLine();

	dirty_rect_hud.add(anim->pos);

	return anim;
}

struct ONScripter::ButtonLink *ONScripter::getSelectableSentence(char *buffer, Fontinfo *info, bool flush_flag, bool nofile_flag, bool skip_whitespace) {
	ButtonLink *button_link  = new ButtonLink();
	button_link->button_type = ButtonLink::TMP_SPRITE_BUTTON;
	button_link->show_flag   = true;

	AnimationInfo *anim      = getSentence(buffer, info, 2, flush_flag,
                                      nofile_flag, skip_whitespace);
	anim->type               = SPRITE_BUTTONS;
	button_link->anim        = anim;
	button_link->select_rect = button_link->image_rect = anim->pos;

	return button_link;
}

void ONScripter::decodeExbtnControl(const char *ctl_str, GPU_Rect *check_src_rect, GPU_Rect *check_dst_rect) {
	char sound_name[256];
	int i, sprite_no, sprite_no2, cell_no, angle;

	while (char com = *ctl_str++) {
		if (com == 'C' || com == 'c') {
			sprite_no  = getNumberFromBuffer(&ctl_str);
			sprite_no2 = sprite_no;
			cell_no    = -1;
			if (*ctl_str == '-') {
				ctl_str++;
				sprite_no2 = getNumberFromBuffer(&ctl_str);
			}
			for (i = sprite_no; i <= sprite_no2; i++)
				refreshSprite(i, false, cell_no, nullptr, nullptr);
		} else if (com == 'P' || com == 'p') {
			sprite_no = getNumberFromBuffer(&ctl_str);
			if (*ctl_str == ',') {
				ctl_str++;
				cell_no = getNumberFromBuffer(&ctl_str);
			} else
				cell_no = 0;
			refreshSprite(sprite_no, true, cell_no, check_src_rect, check_dst_rect);
		} else if (com == 'S' || com == 's') {
			sprite_no = getNumberFromBuffer(&ctl_str);
			if (sprite_no < 0)
				sprite_no = 0;
			else if (sprite_no >= ONS_MIX_CHANNELS)
				sprite_no = ONS_MIX_CHANNELS - 1;
			if (*ctl_str != ',')
				continue;
			ctl_str++;
			if (*ctl_str != '(')
				continue;
			ctl_str++;
			char *buf = sound_name;
			while (*ctl_str != ')' && *ctl_str != '\0') *buf++ = *ctl_str++;
			*buf++ = '\0';
			playSoundThreaded(sound_name, SOUND_CHUNK, false, sprite_no);
			if (*ctl_str == ')')
				ctl_str++;
		} else if (com == 'M' || com == 'm') {
			sprite_no     = getNumberFromBuffer(&ctl_str);
			GPU_Rect rect = sprite_info[sprite_no].pos;
			if (*ctl_str != ',')
				continue;
			ctl_str++; // skip ','
			sprite_info[sprite_no].orig_pos.x = getNumberFromBuffer(&ctl_str);
			if (*ctl_str != ',') {
				UpdateAnimPosXY(&sprite_info[sprite_no]);
				continue;
			}
			ctl_str++; // skip ','
			sprite_info[sprite_no].orig_pos.y = getNumberFromBuffer(&ctl_str);
			UpdateAnimPosXY(&sprite_info[sprite_no]);
			dirtyRectForZLevel(sprite_no, rect);
			sprite_info[sprite_no].visible = true;
			dirtyRectForZLevel(sprite_no, sprite_info[sprite_no].pos);
		} else if (com == 'R' || com == 'r') {
			sprite_no = validSprite(getNumberFromBuffer(&ctl_str));
			if (*ctl_str != ',')
				errorAndExit("Missing angle");
			ctl_str++;
			angle = getNumberFromBuffer(&ctl_str);
			if (*ctl_str != ',')
				errorAndExit("Missing visibility tag");
			ctl_str++;
			sprite2_info[sprite_no].rot     = angle;
			sprite2_info[sprite_no].visible = getNumberFromBuffer(&ctl_str);
			sprite2_info[sprite_no].calcAffineMatrix(window.script_width, window.script_height);
			dirtySpriteRect(sprite_no, true);
		} else if (com == 'E' || com == 'e') {
			sprite_no = validSprite(getNumberFromBuffer(&ctl_str));
			if (*ctl_str != ',')
				errorAndExit("Missing extended cell");
			ctl_str++;
			cell_no = getNumberFromBuffer(&ctl_str);

			sprite2_info[sprite_no].setCell(cell_no);
			sprite2_info[sprite_no].calcAffineMatrix(window.script_width, window.script_height);
			dirtySpriteRect(sprite_no, true);
		}
	}
}

void ONScripter::loadCursor(int no, const char *str, int x, int y, bool abs_flag) {
	cursor_info[no].type = SPRITE_CURSOR;
	cursor_info[no].id   = no;
	cursor_info[no].setImageName(str);
	cursor_info[no].orig_pos.x = x;
	cursor_info[no].orig_pos.y = y;
	UpdateAnimPosXY(&cursor_info[no]);

	parseTaggedString(&cursor_info[no]);
	setupAnimationInfo(&cursor_info[no]);
	if (filelog_flag)
		script_h.findAndAddLog(script_h.log_info[ScriptHandler::FILE_LOG], cursor_info[no].file_name, true); // a trick for save file
	cursor_info[no].abs_flag = abs_flag;
	if (cursor_info[no].gpu_image)
		cursor_info[no].visible = true;
	else
		cursor_info[no].remove();
}

void ONScripter::saveAll(bool no_error) {
	// only save the game state if save_path is set
	if (script_h.save_path != nullptr) {
		saveEnvData();
		saveGlovalData(no_error);
		saveReadLabels("readlbl.sav");
		if (filelog_flag)
			writeLog(script_h.log_info[ScriptHandler::FILE_LOG]);
		if (labellog_flag)
			writeLog(script_h.log_info[ScriptHandler::LABEL_LOG]);
		if (kidokuskip_flag)
			script_h.saveKidokuData(no_error);
	}
}

void ONScripter::loadEnvData() {
	volume_on_flag = true;
	script_h.setStr(&default_env_font, nullptr);
	cdaudio_on_flag    = true;
	kidokumode_flag    = true;
	use_default_volume = true;
	bgmdownmode_flag   = false;
	script_h.setStr(&savedir, nullptr);
	automode_time = 1000;

	if (loadFileIOBuf("envdata") == 0) {
		use_default_volume = false;
#if defined(IOS) || defined(DROID)
		bool goFullscreen = read32s() == 1;
#else
		bool goFullscreen = read32s() == 1 && ons_cfg_options.find("window") == ons_cfg_options.end();
#endif
		if (goFullscreen && window.changeMode(true, true, 1))
			fillCanvas(true, true);
		if (read32s() == 0)
			volume_on_flag = false;
		read32s();
		read32s();
		readStr(&default_env_font);
		if (default_env_font == nullptr)
			script_h.setStr(&default_env_font, DEFAULT_ENV_FONT);
		if (read32s() == 0)
			cdaudio_on_flag = false;
		//read and validate sound volume settings
		for (auto vol : {&voice_volume, &se_volume, &music_volume, &video_volume}) {
			*vol = DEFAULT_VOLUME - read32s();
			if (*vol > DEFAULT_VOLUME)
				*vol = DEFAULT_VOLUME;
		}

		if (read32s() == 0)
			kidokumode_flag = false;
		if (read32s() == 1) {
			bgmdownmode_flag = true;
		}
		readStr(&savedir);
		if (savedir)
			script_h.setSavedir(savedir);
		else
			script_h.setStr(&savedir, ""); //prevents changing savedir
		automode_time = read32s();
	} else {
		script_h.setStr(&default_env_font, DEFAULT_ENV_FONT);
		voice_volume = se_volume = music_volume = video_volume = DEFAULT_VOLUME;
	}
	// set the volumes of channels
	channelvolumes[0] = voice_volume;
	for (int i = 1; i < ONS_MIX_CHANNELS; i++)
		channelvolumes[i] = se_volume;

	//use preferred automode_time, if set
	if (preferred_automode_time_set)
		automode_time = preferred_automode_time;
}

void ONScripter::saveEnvData() {
	file_io_buf.clear();
	write32s(window.getFullscreen());
	write32s(volume_on_flag ? 1 : 0);
	write32s(0);
	write32s(0);
	writeStr(default_env_font);
	write32s(cdaudio_on_flag ? 1 : 0);
	write32s(DEFAULT_VOLUME - voice_volume);
	write32s(DEFAULT_VOLUME - se_volume);
	write32s(DEFAULT_VOLUME - music_volume);
	write32s(DEFAULT_VOLUME - video_volume);
	write32s(kidokumode_flag ? 1 : 0);
	write32s(bgmdownmode_flag ? 1 : 0);
	writeStr(savedir);
	write32s(automode_time);

	saveFileIOBuf("envdata");
}

int ONScripter::refreshMode() {
	if (display_mode & DISPLAY_MODE_TEXT)
		return refresh_window_text_mode;

	return REFRESH_NORMAL_MODE;
}

void ONScripter::cleanLabel() {
	if (initialised() && async.initialised()) {
		auto layer = getLayer<MediaLayer>(video_layer, false);
		if (layer)
			layer->stopPlayback();
	}

	saveAll();

	cleanImages();

	if (seqmusic_info) {
		Mix_HaltMusic();
		Mix_FreeMusic(seqmusic_info);
		seqmusic_info = nullptr;
	}

	if (initialised() && async.initialised()) {
		Lock lock(&playSoundThreadedLock);
		if (music_info) {
			Mix_HaltMusic();
			Mix_FreeMusic(music_info);
			music_info = nullptr;
		}
	}
}

void ONScripter::requestQuit(ExitType code) {
	// Did not we come from a different thread
	if (mainThreadId != 0 && mainThreadId != SDL_GetThreadID(nullptr)) {
		exitCode.store(code, std::memory_order_relaxed);
		throw AsyncController::ThreadTerminate();
	}

	if (canExit) {
		cleanLabel();
		ctrl.quit(code == ExitType::Error ? -1 : 0);
	} else {
		exitCode.store(code, std::memory_order_relaxed);
	}
}

void ONScripter::cleanImages() {
	//GPU Cleanup

	for (GPU_Image **varPtr : {&accumulation_gpu, &hud_gpu,
	                           &text_gpu, &window_gpu, &screenshot_gpu, &draw_gpu, &draw_screen_gpu,
	                           // and some extras put into here for conciseness
	                           &breakup_cellforms_gpu, &breakup_cellform_index_grid, &cursor_gpu, &tmp_image}) {
		GPU_Image *imagePtr = *varPtr;
		if (imagePtr) {
			gpu.freeImage(imagePtr);
			// Don't forget to set the variable pointer contents themselves to zero (i.e. null out accumulation_gpu etc)
			// Absence of this was causing a crash for sentence_font_info on closing
			*varPtr = nullptr;
		}
	}

	for (auto spritePtr : sprites(SPRITE_ALL, true))
		spritePtr->reset();

	for (auto &spriteSet : spritesets) {
		spriteSet.second.setEnable(false);
		spriteSet.second.commit();
	}

	for (GPU_Image **varPtr : {&effect_dst_gpu, &hud_effect_dst_gpu, &effect_src_gpu, &hud_effect_src_gpu}) {
		GPU_Image *imagePtr = *varPtr;
		if (imagePtr) {
			gpu.giveCanvasImage(imagePtr);
			*varPtr = nullptr;
		}
	}

	for (GPU_Image **varPtr : {&onion_alpha_gpu, &pre_screen_gpu, &combined_effect_src_gpu, &combined_effect_dst_gpu}) {
		GPU_Image *imagePtr = *varPtr;
		if (imagePtr) {
			gpu.giveScriptImage(imagePtr);
			*varPtr = nullptr;
		}
	}

	gpu.clearImagePools(true);
}

void ONScripter::disableGetButtonFlag() {
	btndown_flag  = false;
	transbtn_flag = false;

	getzxc_flag       = false;
	gettab_flag       = false;
	getpageup_flag    = false;
	getpagedown_flag  = false;
	getinsert_flag    = false;
	getfunction_flag  = false;
	getenter_flag     = false;
	getcursor_flag    = false;
	spclclk_flag      = false;
	getmclick_flag    = false;
	getskipoff_flag   = false;
	getmouseover_flag = false;
	getmouseover_min = getmouseover_max = 0;
	btnarea_flag                        = false;
	btnarea_pos                         = 0;
}

int ONScripter::getNumberFromBuffer(const char **buf) {
	int ret = 0;
	while (**buf >= '0' && **buf <= '9')
		ret = ret * 10 + *(*buf)++ - '0';

	return ret;
}

bool ONScripter::executeSystemCall(int mode) {
	switch (mode) {
		case SYSTEM_SKIP:
			executeSystemSkip();
			return true;
		case SYSTEM_RESET:
			executeSystemReset();
			return true;
		case SYSTEM_AUTOMODE:
			executeSystemAutomode();
			return true;
		case SYSTEM_SYNC:
			internal_slowdown_counter = 0;
			return true;
	}

	return false;
}

void ONScripter::executeSystemSkip() {
	skip_mode |= SKIP_NORMAL;
	internal_slowdown_counter = 0;
}

void ONScripter::executeSystemAutomode() {
	automode_flag = true;
	skip_mode &= ~SKIP_NORMAL;
	sendToLog(LogLevel::Info, "systemcall_automode: change to automode\n");
}

void ONScripter::executeSystemReset() {
	resetCommand();
}

void ONScripter::executeSystemEnd() {
	endCommand();
}
