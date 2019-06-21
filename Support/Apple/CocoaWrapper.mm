/**
 *  CocoaWrapper.mm
 *  ONScripter-RU
 *
 *  Implements cocoa-specific interfaces.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#import "Support/Apple/CocoaWrapper.hpp"

#ifdef MACOSX

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

@interface CListener : NSObject
{
	@private
	void (*function)();
}

- (id)initWithFunction: (void (*)())ptr;
- (void)action;
- (void)dealloc;

@end

@implementation CListener

- (id)initWithFunction: (void (*)())ptr {
	self = [super init];
	if (self) {
		function = ptr;
	}
	return self;
}

- (void)action {
	function();
}

- (void)dealloc {
	[super dealloc];
}

@end

// Creates a new main menu entry and returns its index
long allocateMenuEntry(const char *name, long atIdx) {
	@autoreleasepool {
		NSMenu *mainMenu = [[NSApplication sharedApplication] mainMenu];
		
		NSMenuItem *subMenuItem = [[NSMenuItem alloc] autorelease];
		
		if (atIdx < 0)
			[mainMenu addItem:subMenuItem];
		else
			[mainMenu insertItem:subMenuItem atIndex:atIdx];
		
		NSString *menuTitle = [NSString stringWithUTF8String:name];
		if (menuTitle) {
			NSMenu *subMenu = [[[NSMenu alloc] initWithTitle:menuTitle] autorelease];
			
			long idx = [mainMenu indexOfItem:subMenuItem];
			
			auto item = [mainMenu itemAtIndex:idx];
			
			if (item) {
				[mainMenu setSubmenu:subMenu forItem:item];
			}
			
			return idx;
		}
		return -1;
	}
}

// Removes an entry from main menu
long deallocateMenuEntry(long idx) {
	@autoreleasepool {
		NSMenu *mainMenu = [[NSApplication sharedApplication] mainMenu];
		
		if ([mainMenu numberOfItems] <= idx) {
			NSLog(@"Exception: Found no menu item by a specified baseIdx");
			return -1;
		}
		
		NSMenuItem *baseMenuItem;
		
		@try {
			baseMenuItem = [mainMenu itemAtIndex:idx];
		} @catch (NSException *) {
			NSLog(@"Exception: Found no menu item by a specified baseIdx");
			return -1;
		}
		
		[mainMenu removeItem:baseMenuItem];
		
		return 0;
	}
}

template <>
long allocateMenuItem(NSMenuItem *baseMenu, const char *name, CocoaAction act, const char *key, long atIdx) {
	@autoreleasepool {
		if (![baseMenu hasSubmenu]) {
			NSLog(@"Exception: Found no sub menu by a specified baseIdx");
			return -1;
		}

		NSMenu *submenu = [baseMenu submenu];
		NSString *itemTitle = [NSString stringWithUTF8String:name];
		NSMenuItem *newItem = nil;
		NSString *keyEquiv = @"";

		if (key)
			keyEquiv = [NSString stringWithUTF8String:key];
		
		SEL action = nil;
		
		if (act.type == CocoaAction::CocoaActionType::SELECTOR) {
			auto selstr = [NSString stringWithUTF8String:act.sel];
			if (selstr) {
				action = NSSelectorFromString(selstr);
			} else {
				NSLog(@"Exception: No valid selector provided");
				return -1;
			}
		} else if (act.type == CocoaAction::CocoaActionType::FUNCTION) {
			action = @selector(action);
		} else {
			NSLog(@"Exception: No action provided");
			return -1;
		}
		
		if (itemTitle && keyEquiv) {
			newItem = [[[NSMenuItem alloc] initWithTitle:itemTitle
												  action:action
										   keyEquivalent:keyEquiv] autorelease];
			
			if (act.type == CocoaAction::CocoaActionType::FUNCTION) {
				CListener *listener = [[[CListener alloc] initWithFunction:act.act] autorelease];
				[newItem setRepresentedObject:listener];
				[newItem setTarget:listener];
			}
			
			if (atIdx < 0)
				[submenu addItem:newItem];
			else
				[submenu insertItem:newItem atIndex:atIdx];
			
			return [submenu indexOfItem:newItem];
		}
		return -1;
	}
}

template <>
long allocateMenuItem(const char *baseMenu, const char *name, CocoaAction act, const char *key, long atIdx) {
	@autoreleasepool {
		NSMenu *mainMenu = [[NSApplication sharedApplication] mainMenu];
		NSString *baseMenuTitle = [NSString stringWithUTF8String:baseMenu];
		if (baseMenuTitle) {
			NSMenuItem *baseMenuItem = [mainMenu itemWithTitle:NSLocalizedString(baseMenuTitle, nil)];
			return allocateMenuItem(baseMenuItem, name, act, key, atIdx);
		}
		return -1;
	}
}

template <>
long allocateMenuItem(long baseMenu, const char *name, CocoaAction act, const char *key, long atIdx) {
	@autoreleasepool {
		NSMenu *mainMenu = [[NSApplication sharedApplication] mainMenu];
		NSMenuItem *baseMenuItem = [mainMenu itemAtIndex:baseMenu];
		return allocateMenuItem(baseMenuItem, name, act, key, atIdx);
	}
}

// Removes an entry from submenu
long deallocateMenuItem(long baseIdx, long itemIdx) {
	@autoreleasepool {
		NSMenu *mainMenu = [[NSApplication sharedApplication] mainMenu];
	
		if ([mainMenu numberOfItems] <= baseIdx) {
			NSLog(@"Exception: Found no menu item by a specified baseIdx");
			return -1;
		}
	
		NSMenuItem *baseMenuItem;
		
		@try {
			baseMenuItem = [mainMenu itemAtIndex:baseIdx];
		} @catch (NSException *) {
			NSLog(@"Exception: Found no menu item by a specified baseIdx");
			return -1;
		}
		
		if (![baseMenuItem hasSubmenu]) {
			NSLog(@"Exception: Found no sub menu by a specified baseIdx");
			return -1;
		}
		
		NSMenu *baseMenu = [baseMenuItem submenu];
		
		if ([baseMenu numberOfItems] <= itemIdx) {
			NSLog(@"Exception: Found no sub menu item by a specified itemIdx");
			return -1;
		}
		
		NSMenuItem *thisMenuItem;
		
		@try {
			thisMenuItem = [baseMenu itemAtIndex:itemIdx];
		} @catch (NSException *) {
			NSLog(@"Exception: Found no sub menu item by a specified itemIdx");
			return -1;
		}
		
		[baseMenu removeItem:thisMenuItem];
		
		return 0;
	}
}

// Enables Toggle Full Screen menu item
long enableFullscreen(void (*act)(), long setBaseIdx) {
	@autoreleasepool {
		NSMenu *mainMenu = [[NSApplication sharedApplication] mainMenu];
		
		NSMenu *viewMenu {nullptr};
		long baseIdx {-1};
		
		@try {
			NSMenuItem *viewMenuItem = [mainMenu itemWithTitle:NSLocalizedString(@"View", nil)];
			baseIdx = [mainMenu indexOfItem:viewMenuItem];
			if (!viewMenuItem || ![viewMenuItem hasSubmenu] || baseIdx != setBaseIdx) {
				if (baseIdx >= 0) deallocateMenuEntry(baseIdx);
				@throw [[NSException alloc] autorelease];
			}
		} @catch (NSException *) {
			baseIdx = allocateMenuEntry([NSLocalizedString(@"View", nil) UTF8String], setBaseIdx);
		}
		
		NSMenuItem *fullScreenItem;
		
		@try {
			viewMenu = [[mainMenu itemAtIndex:baseIdx] submenu];
			fullScreenItem = [viewMenu itemWithTitle:NSLocalizedString(@"Toggle Full Screen", nil)];
			if (!fullScreenItem) @throw [[NSException alloc] autorelease];
		} @catch (NSException *) {
			allocateMenuItem(baseIdx, [NSLocalizedString(@"Toggle Full Screen", nil) UTF8String], {act});
			return 0;
		}
		
		CListener *listener = [[[CListener alloc] initWithFunction:act] autorelease];
		[fullScreenItem setRepresentedObject:listener];
		[fullScreenItem setTarget:listener];
		[fullScreenItem setAction:@selector(action)];
		[fullScreenItem setKeyEquivalent:@""];
		
		//Collides with our events
		//[[[[mainMenu itemAtIndex:baseIdx] submenu] itemAtIndex:itemIdx] setKeyEquivalent:@"f"];
		
		return 0;
	}
}

void nativeCursorState(bool show) {
	static bool currentState {true};
	if (show == currentState) return;
	currentState = show;
	if (show) {
		[NSCursor unhide];
	} else {
		[NSCursor hide];
	}
}

#endif
