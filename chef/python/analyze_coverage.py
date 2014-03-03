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


"""Coverage analysis."""


__author__ = "stefan.bucur@epfl.ch (Stefan Bucur)"


import argparse
import cStringIO
import datetime
import json
import logging
import operator
import os
import re
import sys
import traceback

import coverage

from chef import overlay
from chef import support
from chef import symbex


EXPERIMENTS_FILE = "experiments.txt"


class CoverageReport(object):
    gae_path = "/usr/local/google_appengine"
    cov_line_re = re.compile(r"""^(.+\S+)\s+  # The file name
                                  (\d+)\s+    # No. of statements
                                  (\d+)\s+    # Missed statements
                                  (\d+)%\s*$  # Total coverage""", re.X)
    
    time_bucketing = 60  # Every minute

    def __init__(self, exp_dir, source_file, overlay):
        self._exp_dir = exp_dir
        self._overlay = overlay
        self._source_file = source_file

        self._cov = coverage.coverage(cover_pylib=True, branch=False,
                                      config_file=None,  # Ignore the configuration file
                                      source=None,
                                      omit=[self.gae_path + "/google/*"],
                                      include=None)

    def _reportCoverageJSON(self):
        result = {}

        buffer = cStringIO.StringIO()
        self._cov.report(morfs=None, show_missing=False,
                         file=buffer,
                         omit=None, include=None)

        for line in buffer.getvalue().splitlines():
            match = self.cov_line_re.match(line)
            if not match:
                continue

            file_name = match.group(1)
            if file_name == "TOTAL":
                continue # XXX: Hack, hack, there might be a file named "TOTAL"
            logging.info("  Processing coverage for '%s'" % file_name)
            
            # XXX: Not very nice either, but the coverage module is quite
            # cumbersome to use for non-trivial tasks.
            analysis = self._cov.analysis2(file_name + ".py")
            
            result[file_name] = {
                "executable": analysis[1],
                "excluded": analysis[2],
                "missing": analysis[3],
            }

        buffer.close()

        return result
    
    def _getTimeBuckets(self, alternates):
        buckets = {}
        for alternate in alternates:
            delta = int(alternate[4][0] / 1000000)
            bucket = delta / self.time_bucketing
            buckets[bucket] = buckets.get(bucket, 0) + 1

        if not buckets:
            return {0: 0}
        
        result = {}
        accum = 0
        
        for bucket in range(max(buckets.keys()) + 1):
            accum += buckets.get(bucket, 0)
            result[bucket * self.time_bucketing] = accum

        return result
        
    def computeCoverage(self):
        try:
            with open(os.path.join(self._exp_dir, "s2e-out",
                                   "test_cases.dat"), "r") as f:
                test_case_data = f.read()
                
            alternates = symbex.decodetc(test_case_data)
        except IOError:
            alternates = []
            
        try:
            with open(os.path.join(self._exp_dir, "s2e-out",
                                   "ignored.dat"), "r") as f:
                test_case_data = f.read()
            ignored = symbex.decodetc(test_case_data)
        except IOError:
            ignored = []
        
        with open(self._source_file, "r") as f:
            test_code = f.read()
        
        environment = support.TestEnvironment(test_code)
        test_overlay = environment.getTestOverlays()[self._overlay]
        test_instance = test_overlay(stop_on_error=True)
        test_instance.setUp()
        test_cases = [test_instance.constructAlternate(test_instance.initial_test_input,
                                                       alt) for alt in alternates]
        errors = []
        
        self._cov.erase()
        logging.info("Replaying explored test cases")
        self._cov.start()
        for test_input in test_cases:
            logging.info("Replaying %s" % test_input)
            try:
                test_instance.runConcrete(test_input)
            except:
                logging.exception("Error detected.")
                errors.append((sys.exc_info()[0].__name__,
                               str(test_input),
                               repr(traceback.format_exc())))
        self._cov.stop()
        
        result = {
            "coverage": self._reportCoverageJSON(),
            "test_cases": {
                "total": len(test_cases),
                "ignored": len(ignored),
                "errors": errors,
            },
            "time_bucketing": {
                "generated": self._getTimeBuckets(alternates),
                "ignored": self._getTimeBuckets(ignored),
            }
        }
        
        with open(os.path.join(self._exp_dir, "report.json"), "w") as f:
            json.dump(result, f, indent=True, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser("Coverage analysis tool")
    parser.add_argument("input_dir",
                        help="Input directory for experimental data")
    
    args = parser.parse_args()
    
    logging.basicConfig(level=logging.INFO, format='** %(message)s')
    
    with open(EXPERIMENTS_FILE, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            package, source_file, overlay = line.split()[:3]
            
            for strategy in ["cupa", "randsel", "cupa-prio"]:
                exp_dir = os.path.join(args.input_dir, strategy, overlay)
                if not os.path.exists(exp_dir):
                    continue

                report = CoverageReport(exp_dir, source_file, overlay)
                report.computeCoverage()


if __name__ == "__main__":
    main()
