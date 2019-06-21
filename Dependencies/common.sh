#!/bin/bash

# Control sequences for color output.  We should probably detect color
# capability here and zero these if the terminal doesn't have it.  MinGW
# unfortunately doesn't have `tput' by default, which would make that too
# easy.

NORMAL="\033[0m"
BLUE="\033[0;34m"
GREEN="\033[0;32m"
CYAN="\033[0;36m"
RED="\033[0;31m"
PURPLE="\033[0;35m"
BROWN="\033[0;33m"
GRAY="\033[0;37m"

# These functions provide pretty output from the script.  They all work like
# printf, taking a format string and arguments.

msg() {
    local fs=$1; shift
    if [ -z "$BUILDPKG_ARCH" ]; then
        printf "${GREEN}==>${NORMAL} $fs\n" "$@"
    else
        printf "${CYAN}(%6s) ${GREEN}==>${NORMAL} $fs\n" "$BUILDPKG_ARCH" "$@"
    fi
}

msg2() {
    local fs=$1; shift
    if [ -z "$BUILDPKG_ARCH" ]; then
        printf "${BLUE} ->${NORMAL} $fs\n" "$@"
    else
        printf "${CYAN}(%6s) ${BLUE} ->${NORMAL} $fs\n" "$BUILDPKG_ARCH" "$@"
    fi
}

msg_start() {
    local fs=$1; shift
    if [ -z "$BUILDPKG_ARCH" ]; then
        printf "${PURPLE}>>>${NORMAL} $fs\n" "$@"
    else
        printf "${CYAN}(%6s) ${PURPLE}>>>${NORMAL} $fs\n" "$BUILDPKG_ARCH" "$@"
    fi
}

msg_stop() {
    local fs=$1; shift
    if [ -z "$BUILDPKG_ARCH" ]; then
        printf "${PURPLE}<<<${NORMAL} $fs\n" "$@"
    else
        printf "${CYAN}(%6s) ${PURPLE}<<<${NORMAL} $fs\n" "$BUILDPKG_ARCH" "$@"
    fi
}

warn() {
    local fs=$1; shift
    printf "${BROWN}WARNING:${NORMAL} $fs\n" "$@"
}

error_out() {
    local fs=$1; shift
    printf "${RED}ERROR:${NORMAL} $fs\n" "$@"
    exit 1
}

error() {
    local fs=$1; shift
    printf "${RED}ERROR:${NORMAL} $fs\n" "$@"
}

trim() {
    local var=$@
    var="${var#"${var%%[![:space:]]*}"}"   # remove leading whitespace characters
    var="${var%"${var##*[![:space:]]}"}"   # remove trailing whitespace characters
    echo -n "$var"
}

# Portable replacement for !

not() {
    if $1; then false; else true; fi
}


