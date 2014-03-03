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


"""Measure coverage in an experiment directory.

The script assumes an <exp_dir>/<config>/<target> structure."""

__author__ = "stefan.bucur@epfl.ch (Stefan Bucur)"


import argparse
import os
import subprocess
import sys
import yaml


COVERAGE_FILE = "coverage.json"


def measure_coverage(batch_data, exp_dir, force=False):
    for job in batch_data["jobs"]:
            for config in batch_data["config"]:
                client_dir = os.path.join(exp_dir, config["name"], job["name"])
                if not os.path.isdir(client_dir):
                    print >>sys.stderr, "%s: Directory not found. Skipping." % client_dir
                    continue
                coverage_path = os.path.join(client_dir, COVERAGE_FILE)
                if os.path.exists(coverage_path) and not force:
                    print >>sys.stderr, "%s: Coverage already computed. Skipping." % client_dir
                    continue

                # Re-execute the job command with the output directory as a parameter
                # XXX: Do this in a more transparent way, perhaps?
                subprocess.check_call(" ".join([job["command"], client_dir]), shell=True)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Compute coverage data inside an experiment directory")
    parser.add_argument("batch_file", type=argparse.FileType("r"),
                        help="The experiment platform")
    parser.add_argument("exp_dirs", nargs='+',
                        help="The experiment directory")
    parser.add_argument("-f", "--force", action="store_true", default=False,
                        help="Force coverage recomputation")

    args = parser.parse_args()
    batch_data = yaml.load(args.batch_file)

    for exp_dir in args.exp_dirs:
        if not os.path.isdir(exp_dir):
            parser.error("%s: Invalid experiment directory" % exp_dir)
        measure_coverage(batch_data, exp_dir, args.force)


