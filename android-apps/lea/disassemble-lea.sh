#!/bin/sh

#####################################################################
This script disassembles our Evaluation App LEA with Baksmali.

Baksmali is not included in Leakalizer. You can download it here:
https://code.google.com/p/smali/
#####################################################################

java -jar ~/Downloads/baksmali-1.2.6.jar ./bin/lea.apk -r ALL -f -d /path/to/frameworkjars/system/framework/
