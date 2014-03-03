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

"""Symbolic tests for ASPLOS'14."""

__author__ = "stefan.bucur@epfl.ch (Stefan Bucur)"

# TODO: Rename this file more appropriately.


import argparse
import cStringIO
import importlib
import os
import sys

from chef import light

# TODO: Devise more meaningful defaults.
# Idea: use a method to transform a concrete input into a wildcard of the same length.


class ConfigParserTest(light.SymbolicTest):
    def setUp(self):
        self.ConfigParser = importlib.import_module("ConfigParser")

    def runTest(self):
        input_string = self.getString("input", '\x00'*10)
        string_file = cStringIO.StringIO(input_string)
        
        config = self.ConfigParser.ConfigParser()
        config.readfp(string_file)
        
        for s in config.sections():
            config.options(s)


class ArgparseTest(light.SymbolicTest):
    def setUp(self):
        self.argparse = importlib.import_module("argparse")
    
    def runTest(self):
        parser = self.argparse.ArgumentParser(description="Symtest")
        parser.add_argument(self.getString("arg1_name", '\x00'*3))
        parser.add_argument(self.getString("arg2_name", '\x00'*3))
        
        parser.parse_args([self.getString("arg1", '\x00'*3),
                           self.getString("arg2", '\x00'*3)])


class HTMLParserTest(light.SymbolicTest):
    def setUp(self):        
        self.HTMLParser = importlib.import_module("HTMLParser")
    
    def runTest(self):
        parser = self.HTMLParser.HTMLParser()
        parser.feed(self.getString("html", '\x00'*15))
        parser.close()


class UrllibTest(light.SymbolicTest):
    def setUp(self):
        self.urllib = importlib.import_module("urllib")
        
    def runTest(self):
        u = self.urllib.urlopen(self.getString("input", '\x00'*10))


class XmlEtreeTest(light.SymbolicTest):
    def setUp(self):
        self.ElementTree = importlib.import_module("xml.etree.ElementTree")
    
    def runTest(self):
        xml = self.getString("xml", '\x00'*15, ascii=True)
        self.ElementTree.fromstring(xml)


################################################################################
# Third-party libraries

class SimpleJSONTest(light.SymbolicTest):
    def setUp(self):
        self.simplejson = importlib.import_module("simplejson")
        
    def runTest(self):
        self.simplejson.loads(self.getString("input", '\x00'*15))


class XLRDTest(light.SymbolicTest):
    def setUp(self):
        self.xlrd = importlib.import_module("xlrd")
        
    def runTest(self):
        i = self.getString("input", '\x00'*20)
        self.xlrd.open_workbook(file_contents=i)


class UnicodeCSVTest(light.SymbolicTest):
    def setUp(self):
        self.unicodecsv = importlib.import_module("unicodecsv")
        
    def runTest(self):        
        f = cStringIO.StringIO(self.getString("input", '\x00'*5))
        r = self.unicodecsv.reader(f, encoding="utf-8")
        for row in r:
            pass
        f.close()


class Jinja2Test(light.SymbolicTest):
    def setUp(self):
        self.jinja2 = importlib.import_module("jinja2")
        
    def runTest(self):
        template = self.jinja2.Template(self.getString("input", '\x00'*10))
        template.render(x="y")


class NZMathMPQSTest(light.SymbolicTest):
    def setUp(self):        
        self.methods = importlib.import_module("nzmath.factor.methods")

    def runTest(self):
        self.methods.mpqs(self.getInt("number", 0, 1 << 30, 0))
        

class NZMathNextPrimeTest(light.SymbolicTest):
    def setUp(self):
        self.prime = importlib.import_module("nzmath.prime")
        
    def runTest(self):
        self.prime.nextPrime(self.getInt("number", 0, 1 << 30, 0))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run tests")
    parser.add_argument("--interactive", "-i", action="store_true", default=False,
                        help="Do not automatically end concolic session")
    parser.add_argument("test",
                        help="The test class to execute")
    parser.add_argument("exp_dir", nargs="?",
                        help="The experiment directory used for replay")
    args = parser.parse_args()
    
    test_class = globals().get(args.test)
    if not (test_class and
            isinstance(test_class, type) and
            issubclass(test_class, light.SymbolicTest)):
        print >>sys.stderr, "Invalid test name '%s'." % args.test
        sys.exit(1)
    
    if args.exp_dir:
        import json
        import logging
        
        logging.basicConfig(level=logging.INFO, format='** %(message)s')
        
        replayer = light.SymbolicTestReplayer(test_class)
        with open(os.path.join(args.exp_dir, "hl_test_cases.dat"), "r") as f:
            replayer.replayFromTestCases(f)
        with open(os.path.join(args.exp_dir, "coverage.json"), "w") as f:
            json.dump(replayer.getCoverageReport(), f, indent=True)
    else:
        light.runSymbolic(test_class, interactive=args.interactive)
