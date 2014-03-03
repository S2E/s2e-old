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

"""Lightweight symbolic test framework."""

__author__ = "stefan.bucur@epfl.ch (Stefan Bucur)"


import cStringIO
import logging
import re
import struct
import sys
import traceback

try:
    from chef import symbex
except ImportError:
    symbex = None  # TODO: Replace with a friendlier stub


class SymbolicTest(object):
    """Base class for symbolic tests"""

    def __init__(self, replay_assgn=None):
        self.replay_assgn = replay_assgn

    def getInt(self, name, default, max_value=None, min_value=None):
        if self.replay_assgn:
            if name not in self.replay_assgn:
                logging.info("Key '%s' not found in assignment. Using default '%s'." % (name, default))
                return default
            return self.replay_assgn[name]
        elif not (max_value is None and min_value is None):
            return symbex.symint(default, name, max_value, min_value)
        else:
            return symbex.symint(default, name)

    def getString(self, name, default, max_size=None, min_size=None, ascii=False):
        if not isinstance(default, basestring):
            raise ValueError("Default value must be string or unicode")

        if self.replay_assgn:
            if name not in self.replay_assgn:
                logging.info("Key '%s' not found in assignment. Using default '%s'." % (name, default))
                return default
            return self.replay_assgn[name]
        elif not (max_size is None and min_size is None):
            value = symbex.symsequence(default, name, max_size, min_size)
        else:
            value = symbex.symsequence(default, name)

        if ascii:
            symbex.assumeascii(value)

        return value
    
    def concretize(self, value):
        if self.replay_assgn:
            return value
        
        return symbex.concrete(value)

    def setUp(self):
        """Called once before the test execution."""
        pass

    def runTest(self):
        pass


def runSymbolic(symbolic_test, max_time=0, interactive=False, **test_args):
    """Runs a symbolic test in symbolic mode"""

    test_inst = symbolic_test(**test_args)
    test_inst.setUp()

    is_error_path = False
    symbex.startconcolic(max_time, not interactive)

    try:
        test_inst.runTest()
    except:
        if interactive:
            traceback.print_exc()
        is_error_path = True
    finally:
        if not interactive:
            symbex.endconcolic(is_error_path)


class SymbolicTestCase(object):
    _assgn_re = re.compile(r'([\w.#]+)=>"((?:\\x[0-9A-Fa-f]{2}|\\.|[^"])*)"')

    def __init__(self, line):
        # Sample valid line:
        # 1404853 0xb7436a28 0 1/1 1/1 -1/-1 arg2_name.s#value=>"\x00\x00\x00" ...

        tokens = line.strip().split(" ", 6)

        self.timestamp = int(tokens[0])
        self.fork_pc = int(tokens[1], 16)

        try:
            self.target_dist = int(tokens[2])
        except ValueError:
            self.target_dist = None
            self.path_divergence_dist = None
            self.cfg_divergence_dist = None
            self.min_dist = None
            self.max_dist = None
            assignment_token = line.strip().split(" ", 2)[2]
        else:
            if tokens[3] == "-/-":
                self.path_divergence_dist = (None, None)
            else:
                self.path_divergence_dist = tuple(map(int, tokens[3].split("/")))

            if tokens[4] == "-/-":
                self.cfg_divergence_dist = (None, None)
            else:
                self.cfg_divergence_dist = tuple(map(int, tokens[4].split("/")))

            self.min_dist, self.max_dist = tuple(map(int, tokens[5].split("/")))

            assignment_token = tokens[6]
            
        self.assgn = {}
        for k, v in self._assgn_re.findall(assignment_token):
            if "." in k:
                name, signature = k.rsplit(".", 1)
                kind, _ = signature.split("#")
            else:
                name = k
                kind = "s"
            value = v.decode("string_escape")
            
            if kind == "i":
                self.assgn[name] = struct.unpack("<i", value)[0]
            else:
                self.assgn[name] = value

    def coversNewHLPath(self):
        return self.path_divergence_dist != (None, None)

    def coversNewHLCode(self):
        return self.cfg_divergence_dist != (None, None)

    @classmethod
    def loadFromFile(cls, test_cases_file):
        return [cls(line) for line in test_cases_file]


class SymbolicTestReplayer(object):
    cov_line_re = re.compile(r"""^(.+\S+)\s+  # The file name
                                  (\d+)\s+    # No. of statements
                                  (\d+)\s+    # Missed statements
                                  (\d+)%\s*$  # Total coverage""", re.X)

    def __init__(self, symbolic_test, measure_cov=True, **test_args):
        self.symbolic_test = symbolic_test
        self.test_args = test_args
        self.errors = []

        self._cov = None
        if measure_cov:
            import coverage
            self._cov = coverage.coverage(cover_pylib=True, branch=False,
                                          config_file=None, source=None)

    def replayFromTestCases(self, test_cases_file):
        if self._cov:
            self._cov.erase()
            self._cov.start()

        test_cases = SymbolicTestCase.loadFromFile(test_cases_file)

        for test_case in test_cases:
            self._replayAssignment(test_case.assgn)

        if self._cov:
            self._cov.stop()

    def _replayAssignment(self, assgn):
        logging.info("Replaying %s" % str(assgn))

        # Construct the test object
        test_inst = self.symbolic_test(replay_assgn=assgn, **self.test_args)
        test_inst.setUp()

        try:
            test_inst.runTest()
        except:
            logging.exception("Error detected")
            self.errors.append((sys.exc_info()[0].__name__,
                                str(assgn),
                                repr(traceback.format_exc())))

    def getCoverageReport(self):
        if not self._cov:
            raise ValueError("Coverage not enabled")
        
        result = {}

        buff = cStringIO.StringIO()
        self._cov.report(morfs=None, show_missing=False,
                         file=buff,
                         omit=None, include=None)

        for line in buff.getvalue().splitlines():
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

        buff.close()

        return result
