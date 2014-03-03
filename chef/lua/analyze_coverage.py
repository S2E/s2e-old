#!/usr/bin/env python
# -*- coding: utf-8 -*-
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

"""Analyze Lua coverage information."""

__author__ = "stefan.bucur@epfl.ch (Stefan Bucur)"


import argparse
import json
import os


def parseLuaCoverageFile(f):
    report = {}
    
    prefix_len = 0
    # Pre-scan the file to discover the size of the line prefix
    for line in f:
        if line.startswith("*"):
            prefix_len = len(line.split()[0])
            break
    
    if not prefix_len:
        raise Exception("Could not discover line prefix length")

    print "Prefix len:", prefix_len
    
    f.seek(0)    
    
    try:
        # Advance to the next file header
        while not f.next().startswith("===="):
            pass
        
        file_name = f.next().strip()
        # Skip the underline
        f.next()
        
        while True:
            line, line_no = f.next().rstrip(), 1
            
            executable = []
            missing = []
            
            while not line.startswith("===="):
                if len(line) < prefix_len:
                    line, line_no = f.next().rstrip(), line_no + 1
                    continue
                prefix = line[:prefix_len].strip()
                if not prefix:
                    line, line_no = f.next().rstrip(), line_no + 1
                    continue
                if prefix.startswith("*"):
                    missing.append(line_no)
                executable.append(line_no)
                
                line, line_no = f.next().rstrip(), line_no + 1
                
            report[file_name] = {
                "executable": executable,
                "missing": missing
            }
            
            file_name = f.next().strip()
            f.next()
            
            if file_name == "Summary":
                break
            
    except StopIteration:
        raise
    
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Analyze coverage info")
    parser.add_argument("test_dir")
    args = parser.parse_args()
    
    with open(os.path.join(args.test_dir, "luacov.report.out"), "r") as f:
        report = parseLuaCoverageFile(f)
        
    with open(os.path.join(args.test_dir, "coverage.json"), "w") as f:
        json.dump(report, f, indent=True)
