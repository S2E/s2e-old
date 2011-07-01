#!/bin/sh

OUT1="$1"
OUT2="$2"
CMD="$3"
diff "$OUT1" "$OUT2"
if [ $? -eq 1 ]; then
  echo "FAILED $CMD"
  exit -1
else
  echo "SUCCESS $CMD"
  exit 0
fi
