#!/bin/bash

#
# tidy.sh
# ONScripter-RU
#
# Run clang-format and clang-tidy on the project.
#
# Consult LICENSE file for licensing terms and copyright holders.
#

# Environment variables:
# PREPROCESSOR - extra preprocessor flags like "-DPUBLIC_RELEASE=1"
# TIDY - path to clang-tidy in a specific place
# FORMAT - path to clang-format in a specific place

if [ "$TIDY" == "" ]; then
	echo "Looking clang-tidy..."
	for path in ${PATH//:/ }; do
		TIDY="$(ls $path/clang-tid*)"
		if [ "$TIDY" != "" ]; then
			break
		fi
	done
	if [ "$TIDY" == "" ] && [ -f "/c/llvm/bin/clang-tidy.exe" ]; then
		TIDY="/c/llvm/bin/clang-tidy.exe"
	fi
fi

if [ "$TIDY" == "" ]; then
    echo "No clang-tidy found!"
    exit 1
fi

if [ "$FORMAT" == "" ]; then
	echo "Looking clang-format..."
	for path in ${PATH//:/ }; do
		FORMAT="$(ls $path/clang-forma*)"
		if [ "$FORMAT" != "" ]; then
			break
		fi
	done
	if [ "$FORMAT" == "" ] && [ -f "/c/llvm/bin/clang-format.exe" ]; then
		FORMAT="/c/llvm/bin/clang-format.exe"
	fi
fi

if [ "$FORMAT" == "" ]; then
    echo "No clang-format found!"
    exit 1
fi

# Most reasonable ones
CHECKS=-checks=*,\
-clang-analyzer-alpha.*,\
-clang-analyzer-security.insecureAPI.rand,\
-llvm-include-order,\
-llvm-header-guard,\
-modernize-use-auto,\
-cert-err58-cpp,\
-cert-dcl50-cpp,\
-readability-braces-around-statements,\
-readability-implicit-bool-cast,\
-cppcoreguidelines-pro-type-member-init,\
-cppcoreguidelines-pro-type-static-cast-downcast,\
-cppcoreguidelines-pro-bounds-pointer-arithmetic,\
-cppcoreguidelines-pro-type-vararg,\
-cppcoreguidelines-pro-bounds-array-to-pointer-decay,\
-cppcoreguidelines-pro-bounds-constant-array-index,\
-cppcoreguidelines-pro-type-reinterpret-cast,\
-cppcoreguidelines-pro-type-union-access,\
-cppcoreguidelines-pro-type-const-cast,\
-google-readability-braces-around-statements,\
-google-readability-todo,\
-google-explicit-constructor,\
-google-default-arguments,\
-google-runtime-references,\
-google-runtime-int

SRCDIR=$(dirname "$0")
pushd $SRCDIR/../ &>/dev/null
SRCDIR=$(pwd)

if [ ! -f "Engine/Core/Loader.cpp" ]; then
	echo "No sources found in $SRCDIR!"
	exit 1
fi

if [ -f "DerivedData/Xcode/onscrlib64h/onscrlib/include/SDL2/SDL_gpu.h" ]; then
	ONSCRLIB="DerivedData/Xcode/onscrlib64h/onscrlib"
elif [ -f "DerivedData/Xcode/onscrlib64/onscrlib/include/SDL2/SDL_gpu.h" ]; then
	ONSCRLIB="DerivedData/Xcode/onscrlib64/onscrlib"
elif [ -f "DerivedData/Xcode/onscrlib32/onscrlib/include/SDL2/SDL_gpu.h" ]; then
	ONSCRLIB="DerivedData/Xcode/onscrlib64/onscrlib"
elif [ -f "DerivedData/MinGW-i686/Dependencies/onscrlib/include/SDL2/SDL_gpu.h" ]; then
	ONSCRLIB="DerivedData/MinGW-i686/Dependencies/onscrlib/onscrlib"
else
	echo "No compiled onscrlib found!"
	exit 1
fi

CPPFILES=$(find Tools Engine Support  \( -name \*.hpp -o -name \*.cpp \))
CFILES=$(find Tools Engine Support -name \*.c)
MMFILES=$(find Tools Engine Support -name \*.mm ! -name UIKitWrapper.mm)

INCLUDES="-I$ONSCRLIB/include -I$ONSCRLIB/include/freetype2"
PREPROCESSOR="$PREPROCESSOR -DOV_EXCLUDE_STATIC_CALLBACKS=1 -DUSE_LUA=1 -D_THREAD_SAFE=1"
case `uname` in
	MINGW32*)
		PREPROCESSOR="$PREPROCESSOR -DWIN32=1 -target i686-w64-mingw32"
	;;
	Darwin*)
		PREPROCESSOR="$PREPROCESSOR -DMACOSX=1"
	;;
	*)
		PREPROCESSOR="$PREPROCESSOR -DLINUX=1"
	;;
esac

if [ "$1" == "-format" ]; then
	printf "\e[31mFormatting C files...\e[0m\n"
	$FORMAT -i $CFILES
	case `uname` in
		Darwin*)
			printf "\e[31mFormatting MM files...\e[0m\n"
			$FORMAT -style=file -i $MMFILES
		;;
	esac
	printf "\e[31mFormatting C++ files...\e[0m\n"
	$FORMAT -i $CPPFILES
else
	printf "\e[31mValidating C files...\e[0m\n"
	$TIDY $CFILES -fix $CHECKS -- -std=c11 $PREPROCESSOR $INCLUDES
	case `uname` in
		Darwin*)
			printf "\e[31mValidating MM files...\e[0m\n"
			$TIDY $MMFILES -fix $CHECKS -- -std=c++14 $PREPROCESSOR $INCLUDES
		;;
	esac
	printf "\e[31mValidating C++ files...\e[0m\n"
	$TIDY $CPPFILES -fix $CHECKS -- -xc++ -std=c++14 $PREPROCESSOR $INCLUDES
fi

popd &>/dev/null

exit 0
