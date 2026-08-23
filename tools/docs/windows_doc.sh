#!/bin/bash
#
# Generates docs for Windows C++/C library
#

CURRENT_DIR="`pwd`"

cd "${CURRENT_DIR}"/../../windows

doxygen
