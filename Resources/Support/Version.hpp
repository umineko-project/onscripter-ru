/**
 *  Version.hpp
 *  ONScripter-RU
 *
 *  Engine version macros.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#define macro_xstr(s) macro_str(s)
#define macro_str(s) #s

//WARNING: Do not forget to update this in Xcode project!
#define VER_NUMBER 20190109-ru
#define ONS_VERSION macro_xstr(VER_NUMBER)
#define ONS_CODENAME macro_xstr(Chamerion)
#define NSC_VERSION 300

#define VERSION_STR1 "ONScripter-RU"
#define VERSION_STR2 "Copyright (C) 2001-2011 Studio O.G.A. Portions copyright 2005-2006 insani, 2006-2009 Haeleth, 2007-2011 \"Uncle\" Mion Sonozaki, 2011-2019 Umineko Project. All Rights Reserved."

/* 
 * API versioning:
 *
 * API_FEATURESET defines the supported commands, increase this 
 * value after adding one. User scripts <= API_FEATURESET are
 * supported.
 *
 * API_COMPAT defines supported api syntax and logic, increase this
 * value after changing command syntax or command logic. User scripts
 * must request a == API_COMPAT,
 *
 * API_PATCH defines non-critical api changes which may and may not 
 * be required by a script. User scripts define a lowest acceptable
 * API_PATCH.
 */
#define API_FEATURESET	2
#define API_COMPAT		2
#define API_PATCH		0

#define ONS_API macro_xstr(API_FEATURESET.API_COMPAT.API_PATCH)
