#!/bin/bash

# The goal here is to regenerate a file signalent.h if it is missing. This file contains only a list of the signals abbreviations.

# This script needs to be improved...
cpp -dM /usr/include/signal.h \
	| tr -s '\t ' '  '  \
	| sed 's:#define \(SIG[A-Z]\+[0-9]*\) \([0-9]\+\):\2 \1:p;d' \
	| sort -n --unique
