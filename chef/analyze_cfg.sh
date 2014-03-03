#!/bin/bash
#
# Copyright (C) 2014 EPFL.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# This scripts converts the sequence of .dot files generated during a
# Chef run into a multi-page PDF illustrating the evolution of the
# HL-CFG extracted.

set -e

function usage() {
    echo "Usage: analyze_cfg.sh"
    echo "  -h   Show this message"
    echo "  -n   The maximum number of files to concatenate"
    echo "  -t   Analyze the symbolic execution tree instead"
}

NUM_FILES=
PREFIX=interp_cfg
while getopts "n:h" OPTION; do
    case $OPTION in
	h)
	    usage >&2
	    exit 1;;
	n)
	    NUM_FILES=$OPTARG;;
	t)
	    PREFIX=interp_tree;;
	?)
	    echo "Unknown parameter." >&2
	    usage >&2
	    exit 1;;
    esac
done

shift $((OPTIND - 1))

EXP_DIR="$1"
OUTPUT_FILE="cfg.pdf"

if [ -z "$EXP_DIR" ]; then
    echo "Must specify experiment dir." >&2
    exit 1
fi

if [ ! -d "${EXP_DIR}/s2e-out" ]; then
    echo "Cannot find S2E output dir." >&2
    exit 1
fi

ALL_FILES=
COUNTER=0

for FILE in ${EXP_DIR}/s2e-out/${PREFIX}*.dot; do
    if [ -n "$NUM_FILES" ]; then
	if (( COUNTER >= NUM_FILES )); then
	    echo "Ignoring files starting from ${FILE}" >&2
	    break
	fi
    fi
    PDF_FILE=${FILE%.dot}.pdf
    if [ ! -f "$PDF_FILE" ]; then
	echo "Generating ${PDF_FILE} ..." >&2
	dot -Tpdf -o "$PDF_FILE" "$FILE"
	if [ ! -f "$PDF_FILE" ]; then
	    echo "Error generating PDF. Exiting." >&2
	    exit 1
	fi
    fi
    ALL_FILES="$ALL_FILES $PDF_FILE"
    COUNTER=$((COUNTER + 1))
done

gs -q -dNOPAUSE -dBATCH -sDEVICE=pdfwrite -sOutputFile=${OUTPUT_FILE} ${ALL_FILES}

echo "Wrote ${OUTPUT_FILE}" >&2
