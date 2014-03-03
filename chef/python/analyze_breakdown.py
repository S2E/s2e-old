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

"""Script for analyzing the test case effectiveness."""

__author__ = "stefan.bucur@epfl.ch (Stefan Bucur)"


import argparse
import os

import matplotlib
matplotlib.use("Cairo")
import matplotlib.pyplot as plt
import numpy as np

from chef import light

USEC_IN_SEC = 1000000
SEC_IN_MIN = 60


matplotlib.rc('font', family='Arial')


class TestCaseAnalyzer(object):
    
    @classmethod
    def _binSequence(cls, sequence, bucket_fn, count_only=False):
        """Partition a sequence into buckets of the same value."""
        result = {}
        for item in sequence:
            bucket = bucket_fn(item)
            if count_only:
                result[bucket] = result.get(bucket, 0) + 1
            else:
                result.setdefault(bucket, []).append(item)
        return result
    
    @classmethod
    def _getSuccessRate(cls, test_cases, relative=True):
        """Count the test cases covering new code or new paths."""
        if not test_cases:
            return (0, 0, 0)
        
        new_paths_count = sum(1 for tc in test_cases if tc.coversNewHLPath())
        new_code_count = sum(1 for tc in test_cases if tc.coversNewHLCode())
        
        if not relative:
            return (len(test_cases),
                    new_paths_count,
                    new_code_count)
        else:
            return (1.0,
                    float(new_paths_count)/len(test_cases),
                    float(new_code_count)/len(test_cases))
    
    def __init__(self, test_cases):
        self.test_cases = test_cases
        
        
    def getSuccessRate(self, bucket_fn=None, relative=True):
        if not bucket_fn:
            return self._getSuccessRate(self.test_cases)
        
        bins = self._binSequence(self.test_cases, bucket_fn,
                                 count_only=False)
        
        min_bin = min(bins.iterkeys())
        max_bin = max(bins.iterkeys())
        
        values = range(min_bin, max_bin+1)
        
        return [(v,) + self._getSuccessRate(bins.get(v, []), relative=relative)
                for v in values]


def plotSuccessBreakdown(test_cases):
    """Break the test case set into multiple buckets, and plot the success
    rate within each bucket."""
    
    MAX_DISTANCE = 16
    
    BAR_WIDTH = 0.8
    
    analyzer = TestCaseAnalyzer(test_cases)
    
    relative_data = analyzer.getSuccessRate(lambda tc: min(tc.target_dist, MAX_DISTANCE),
                                            relative=True)
    
    relative_data = [np.array(a) for a in zip(*relative_data)]
    
    plt.figure(figsize=(5, 10), dpi=300)
    plt.suptitle("Breakdown by DistToUncovered")
    
    plt.subplot(3, 1, 1)
    plt.title("Test Case Distribution (Relative)")
    plt.xticks(relative_data[0])
    plt.ylabel("Fraction of TC in bucket")
    
    plt.bar(relative_data[0] - BAR_WIDTH/2,
            relative_data[3], width=BAR_WIDTH,
            color="r", label="New CFG")
    plt.bar(relative_data[0] - BAR_WIDTH/2,
            relative_data[2] - relative_data[3], width=BAR_WIDTH,
            bottom=relative_data[3],
            color="g", label="New Paths")
    plt.bar(relative_data[0] - BAR_WIDTH/2,
            relative_data[1] - relative_data[2], width=BAR_WIDTH,
            bottom=relative_data[2],
            color="b", label="All Paths")
    plt.legend(loc="upper right")
    
    absolute_data = analyzer.getSuccessRate(lambda tc: min(tc.target_dist, MAX_DISTANCE),
                                            relative=False)
    absolute_data = [np.array(a) for a in zip(*absolute_data)]
    
    plt.subplot(3, 1, 2)
    plt.title("Test Case Distribution (Absolute)")
    plt.xticks(absolute_data[0])
    plt.ylabel("Count")
    
    plt.bar(absolute_data[0] - BAR_WIDTH/2,
            absolute_data[3], width=BAR_WIDTH,
            color="r", label="New CFG")
    plt.bar(absolute_data[0] - BAR_WIDTH/2,
            absolute_data[2] - absolute_data[3], width=BAR_WIDTH,
            bottom=absolute_data[3],
            color="g", label="New Paths")
    plt.bar(absolute_data[0] - BAR_WIDTH/2,
            absolute_data[1] - absolute_data[2], width=BAR_WIDTH,
            bottom=absolute_data[2],
            color="b", label="All Paths")
    plt.legend(loc="upper right")
    
    dist_data = [a / np.sum(a, dtype=np.float) for a in absolute_data]
    
    plt.subplot(3, 1, 3)
    plt.title("Success Rate Distribution")
    plt.xticks(absolute_data[0])
    plt.xlabel("Bins")
    
    plt.bar(absolute_data[0] - BAR_WIDTH/2,
            dist_data[3], width=BAR_WIDTH/3,
            color="r", label="New CFG")
    plt.bar(absolute_data[0] - BAR_WIDTH/6,
            dist_data[2], width=BAR_WIDTH/3,
            color="g", label="New Paths")
    plt.bar(absolute_data[0] + BAR_WIDTH/6,
            dist_data[1], width=BAR_WIDTH/3,
            color="b", label="All Paths")
    plt.legend(loc="upper right")
    
    plt.show()
    plt.savefig("breakdown.eps")


def plotGenerationRate(test_cases):
    filters = [
      lambda tc: True,
      lambda tc: tc.coversNewHLPath(),
      lambda tc: tc.coversNewHLCode(),
    ]
    
    x = [np.array([float(tc.timestamp) / USEC_IN_SEC / SEC_IN_MIN
                   for tc in test_cases if f(tc)])
         for f in filters]
    y = [np.arange(1, sum(1 for tc in test_cases if f(tc)) + 1)
         for f in filters]

    plt.plot(x[0], y[0], linestyle='-', marker='.', label="All")
    plt.plot(x[1], y[1], linestyle='-', marker='.', label="New Paths")
    plt.plot(x[2], y[2], linestyle='-', marker='.', label="New CFG")
    
    plt.title("Test case generation in time")
    plt.xlabel("Time [minutes]")
    plt.ylabel("# of test cases")
    plt.grid(True)
    plt.legend(loc='upper left')
    plt.show()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Analyze test information")
    parser.add_argument("command", choices=['breakdown'])
    parser.add_argument("test_dir",
                        help="The S2E output directory")
    
    args = parser.parse_args()
    
    if args.command == 'breakdown':
        test_cases = light.SymbolicTestCase.loadFromFile(
            os.path.join(args.test_dir, 'all_test_cases.dat'))    
        # plotGenerationRate(test_cases)
        plotSuccessBreakdown(test_cases)
    elif args.command == 'features':
        pass