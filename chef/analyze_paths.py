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

"""Analyze path generation effectiveness for an experiment."""

__author__ = "stefan.bucur@epfl.ch (Stefan Bucur)"


import argparse
import os
import subprocess
import sys

from prettytable import PrettyTable


RUN_STATS_TIME_INDEX = 14


def getWallTime(job_dir):
    last_stat_line = subprocess.check_output(["tail", "-1",
                                              os.path.join(job_dir, "run.stats")])
    wall_time = float(last_stat_line.strip().lstrip("(").rstrip(")")
                      .split(",")[RUN_STATS_TIME_INDEX])
    return wall_time


def getPathCounts(job_dir):
    counts = []
    for path_file in ["all_test_cases.dat",
                      "hl_test_cases.dat",
                      "cfg_test_cases.dat"]:
        count_line = subprocess.check_output(
            ["wc", "-l", os.path.join(job_dir, path_file)])
        counts.append(int(count_line.split()[0]))
    return tuple(counts)


def main():
    parser = argparse.ArgumentParser(
        description="Analyze path generation for an experiment.")
    parser.add_argument("expdir",
                        help="The experiment path")

    args = parser.parse_args()

    if not os.path.isdir(args.expdir):
        print >>sys.stderr, "Invalid experiment path."
        exit(1)

    for config in sorted([f for f in os.listdir(args.expdir)
                          if os.path.isdir(os.path.join(args.expdir, f))]):
        print config
        config_dir = os.path.join(args.expdir, config)

        table = PrettyTable(["Name", "Time (s)", "Time/LLpath (ms)", "Time/HLpath (ms)", "All paths", "HL paths", "CFG paths"])
        table.align = "r"
        table.float_format = ".2"

        for job in sorted([f for f in os.listdir(config_dir)
                           if os.path.isdir(os.path.join(config_dir, f))]):
            job_dir = os.path.join(config_dir, job)

            wall_time = getWallTime(job_dir)
            path_counts = getPathCounts(job_dir)
            table.add_row([job, int(wall_time), int(1000*wall_time/path_counts[0]), int(1000*wall_time/path_counts[1]),
                           path_counts[0], path_counts[1], path_counts[2]])

        print table
        print


if __name__ == "__main__":
    main()
