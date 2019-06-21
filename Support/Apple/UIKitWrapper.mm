/**
 *  UIKitWrapper.mm
 *  ONScripter-RU
 *
 *  Implements uikit-specific interfaces.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Support/Apple/UIKitWrapper.hpp"

#ifdef IOS

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <objc/runtime.h>
#include <SDL2/SDL.h>

bool backupDisable(const char *path) {
	NSString *nspath = [[NSString alloc] initWithCString:path encoding:NSUTF8StringEncoding];
	
	NSURL *url = [NSURL fileURLWithPath:nspath];
	BOOL success {NO};
	if ([url checkResourceIsReachableAndReturnError:nil]) {
		NSError *error = nil;
		success = [url setResourceValue:[NSNumber numberWithBool:YES]
									forKey:NSURLIsExcludedFromBackupKey error:&error];
	}

	return success == YES;
}

bool openURL(const char *url) {
	NSString *urlstr = [[NSString alloc] initWithCString:url encoding:NSUTF8StringEncoding];
	NSURL *nsurl = [NSURL URLWithString:urlstr];
	BOOL success {NO};
	if (nsurl)
		success = [[UIApplication sharedApplication] openURL:nsurl];
	return success == YES;
}

static const NSDictionary *keyMap =
@{
	@"`": [NSNumber numberWithInt:SDL_SCANCODE_NONUSHASH],
	@"1": [NSNumber numberWithInt:SDL_SCANCODE_1],
	@"2": [NSNumber numberWithInt:SDL_SCANCODE_2],
	@"3": [NSNumber numberWithInt:SDL_SCANCODE_3],
	@"4": [NSNumber numberWithInt:SDL_SCANCODE_4],
	@"5": [NSNumber numberWithInt:SDL_SCANCODE_5],
	@"6": [NSNumber numberWithInt:SDL_SCANCODE_6],
	@"7": [NSNumber numberWithInt:SDL_SCANCODE_7],
	@"8": [NSNumber numberWithInt:SDL_SCANCODE_8],
	@"9": [NSNumber numberWithInt:SDL_SCANCODE_9],
	@"0": [NSNumber numberWithInt:SDL_SCANCODE_0],
	@"-": [NSNumber numberWithInt:SDL_SCANCODE_MINUS],
	@"=": [NSNumber numberWithInt:SDL_SCANCODE_EQUALS],
	@"й": [NSNumber numberWithInt:SDL_SCANCODE_Q],
	@"ц": [NSNumber numberWithInt:SDL_SCANCODE_W],
	@"у": [NSNumber numberWithInt:SDL_SCANCODE_E],
	@"к": [NSNumber numberWithInt:SDL_SCANCODE_R],
	@"е": [NSNumber numberWithInt:SDL_SCANCODE_T],
	@"н": [NSNumber numberWithInt:SDL_SCANCODE_Y],
	@"г": [NSNumber numberWithInt:SDL_SCANCODE_U],
	@"ш": [NSNumber numberWithInt:SDL_SCANCODE_I],
	@"щ": [NSNumber numberWithInt:SDL_SCANCODE_O],
	@"з": [NSNumber numberWithInt:SDL_SCANCODE_P],
	@"х": [NSNumber numberWithInt:SDL_SCANCODE_LEFTBRACKET],
	@"ъ": [NSNumber numberWithInt:SDL_SCANCODE_RIGHTBRACKET],
	@"ф": [NSNumber numberWithInt:SDL_SCANCODE_A],
	@"ы": [NSNumber numberWithInt:SDL_SCANCODE_S],
	@"в": [NSNumber numberWithInt:SDL_SCANCODE_D],
	@"а": [NSNumber numberWithInt:SDL_SCANCODE_F],
	@"п": [NSNumber numberWithInt:SDL_SCANCODE_G],
	@"р": [NSNumber numberWithInt:SDL_SCANCODE_H],
	@"о": [NSNumber numberWithInt:SDL_SCANCODE_J],
	@"л": [NSNumber numberWithInt:SDL_SCANCODE_K],
	@"д": [NSNumber numberWithInt:SDL_SCANCODE_L],
	@"ж": [NSNumber numberWithInt:SDL_SCANCODE_SEMICOLON],
	@"э": [NSNumber numberWithInt:SDL_SCANCODE_APOSTROPHE],
	@"я": [NSNumber numberWithInt:SDL_SCANCODE_Z],
	@"ч": [NSNumber numberWithInt:SDL_SCANCODE_X],
	@"с": [NSNumber numberWithInt:SDL_SCANCODE_C],
	@"м": [NSNumber numberWithInt:SDL_SCANCODE_V],
	@"и": [NSNumber numberWithInt:SDL_SCANCODE_B],
	@"т": [NSNumber numberWithInt:SDL_SCANCODE_N],
	@"ь": [NSNumber numberWithInt:SDL_SCANCODE_M],
	@"б": [NSNumber numberWithInt:SDL_SCANCODE_KP_LESS],
	@"ю": [NSNumber numberWithInt:SDL_SCANCODE_KP_GREATER],
	@"q": [NSNumber numberWithInt:SDL_SCANCODE_Q],
	@"w": [NSNumber numberWithInt:SDL_SCANCODE_W],
	@"e": [NSNumber numberWithInt:SDL_SCANCODE_E],
	@"r": [NSNumber numberWithInt:SDL_SCANCODE_R],
	@"t": [NSNumber numberWithInt:SDL_SCANCODE_T],
	@"y": [NSNumber numberWithInt:SDL_SCANCODE_Y],
	@"u": [NSNumber numberWithInt:SDL_SCANCODE_U],
	@"i": [NSNumber numberWithInt:SDL_SCANCODE_I],
	@"o": [NSNumber numberWithInt:SDL_SCANCODE_O],
	@"p": [NSNumber numberWithInt:SDL_SCANCODE_P],
	@"[": [NSNumber numberWithInt:SDL_SCANCODE_LEFTBRACKET],
	@"]": [NSNumber numberWithInt:SDL_SCANCODE_RIGHTBRACKET],
	@"a": [NSNumber numberWithInt:SDL_SCANCODE_A],
	@"s": [NSNumber numberWithInt:SDL_SCANCODE_S],
	@"d": [NSNumber numberWithInt:SDL_SCANCODE_D],
	@"f": [NSNumber numberWithInt:SDL_SCANCODE_F],
	@"g": [NSNumber numberWithInt:SDL_SCANCODE_G],
	@"h": [NSNumber numberWithInt:SDL_SCANCODE_H],
	@"j": [NSNumber numberWithInt:SDL_SCANCODE_J],
	@"k": [NSNumber numberWithInt:SDL_SCANCODE_K],
	@"l": [NSNumber numberWithInt:SDL_SCANCODE_L],
	@";": [NSNumber numberWithInt:SDL_SCANCODE_SEMICOLON],
	@"'": [NSNumber numberWithInt:SDL_SCANCODE_APOSTROPHE],
	@"z": [NSNumber numberWithInt:SDL_SCANCODE_Z],
	@"x": [NSNumber numberWithInt:SDL_SCANCODE_X],
	@"c": [NSNumber numberWithInt:SDL_SCANCODE_C],
	@"v": [NSNumber numberWithInt:SDL_SCANCODE_V],
	@"b": [NSNumber numberWithInt:SDL_SCANCODE_B],
	@"n": [NSNumber numberWithInt:SDL_SCANCODE_N],
	@"m": [NSNumber numberWithInt:SDL_SCANCODE_M],
	@",": [NSNumber numberWithInt:SDL_SCANCODE_COMMA],
	@".": [NSNumber numberWithInt:SDL_SCANCODE_PERIOD],
	@"/": [NSNumber numberWithInt:SDL_SCANCODE_SLASH],
	@" ": [NSNumber numberWithInt:SDL_SCANCODE_SPACE],
	@"\b": [NSNumber numberWithInt:SDL_SCANCODE_BACKSPACE],
	@"\t": [NSNumber numberWithInt:SDL_SCANCODE_TAB],
	@"\r": [NSNumber numberWithInt:SDL_SCANCODE_RETURN],
	UIKeyInputUpArrow: [NSNumber numberWithInt:SDL_SCANCODE_UP],
	UIKeyInputDownArrow: [NSNumber numberWithInt:SDL_SCANCODE_DOWN],
	UIKeyInputLeftArrow: [NSNumber numberWithInt:SDL_SCANCODE_LEFT],
	UIKeyInputRightArrow: [NSNumber numberWithInt:SDL_SCANCODE_RIGHT],
	UIKeyInputEscape: [NSNumber numberWithInt:SDL_SCANCODE_ESCAPE],
	// These two are also exported but via non-public symbols with the same names.
	@"UIKeyInputPageUp": [NSNumber numberWithInt:SDL_SCANCODE_PAGEUP],
	@"UIKeyInputPageDown": [NSNumber numberWithInt:SDL_SCANCODE_PAGEDOWN],
};
static NSMutableArray *keyCommandMap {nil};
static NSString *currentPressed {nil};
static KeySender keySender {nil};

static NSArray *keyCommands(id * /* that */, SEL /* sel */) {
	if (currentPressed) {
		//NSLog(@"Released %@", currentPressed);
		
		auto event = new SDL_Event{};
		event->type = SDL_KEYUP;
		event->key.state = SDL_RELEASED;
		event->key.keysym.scancode = (SDL_Scancode)[[keyMap objectForKey:currentPressed] intValue];
		keySender(event);
		
		currentPressed = nil;
	}
	return keyCommandMap;
}

static void keyHandle(id * /* that */, SEL /* sel */, UIKeyCommand *key) {
	auto input = [key input];
	NSNumber *keyCode = [keyMap objectForKey:input];
	if (keyCode) {
		currentPressed = input;
		//NSLog(@"Pressed %@", currentPressed);
		auto event = new SDL_Event{};
		event->type = SDL_KEYDOWN;
		event->key.state = SDL_PRESSED;
		event->key.keysym.scancode = (SDL_Scancode)[keyCode intValue];
		keySender(event);
	} else {
		NSLog(@"Unrecognised key %@", input);
	}
}

void setupKeyboardHandling(KeySender s) {
	keyCommandMap = [[NSMutableArray alloc] init];
	SEL keyHandleSel = sel_registerName("keyHandle:");
	for (NSString *input in keyMap) {
		[keyCommandMap addObject:[UIKeyCommand keyCommandWithInput:input
													 modifierFlags:kNilOptions
															action:keyHandleSel]];
	}
	
	if (class_addMethod(objc_getClass("SDL_uikitviewcontroller"),
						keyHandleSel,
						(IMP)keyHandle,
						"v@:@")) {
		if (class_addMethod(objc_getClass("SDL_uikitviewcontroller"),
							sel_registerName("keyCommands"),
							(IMP)keyCommands,
							"@@:")) {
			keySender = s;
		} else {
			NSLog(@"Failed to insert keyCommands implementation");
		}
	} else {
		NSLog(@"Failed to insert keyHandle implementation");
	}
}

#endif
