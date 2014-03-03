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

"""Lua script runner."""

__author__ = "stefan.bucur@epfl.ch (Stefan Bucur)"


import argparse
import os
import subprocess


DEFAULT_SYMBEXOPT = "lua-noopt"
LCOV_OUT = "luacov.stats.out"
LCOV_REPORT = "luacov.report.out"


def main():
    parser = argparse.ArgumentParser(description="Run Lua scripts.")
    parser.add_argument("test_file",
                        help="The Lua test file")
    parser.add_argument("exp_dir", nargs="?",
                        help="The experiment directory used for replay")
    args = parser.parse_args()
    args.test_file = os.path.abspath(args.test_file)

    symbex_opt = os.environ.setdefault("LUASYMBEXOPT", DEFAULT_SYMBEXOPT)
    this_dir = os.path.abspath(os.path.dirname(__file__))
    install_dir = os.path.join(this_dir, "build", symbex_opt)
    targets_dir = os.path.join(this_dir, "targets")

    os.environ["LUA_PATH"] = ";".join([
        os.path.join(targets_dir, "?.lua"),
        os.path.join(install_dir, "share/lua/5.2/?.lua"),
        os.path.join(install_dir, "share/lua/5.2/?/init.lua"),
        os.path.join(os.environ["HOME"], ".luarocks/share/lua/5.2/?.lua"),
        os.path.join(os.environ["HOME"], ".luarocks/share/lua/5.2/?/init.lua"),
        os.path.join(install_dir, "share/lua/5.2//?.lua"),
        os.path.join(install_dir, "share/lua/5.2//?/init.lua"),
        "/usr/local/share/lua/5.2/?.lua"
        "/usr/local/share/lua/5.2/?/init.lua"
        "/usr/local/lib/lua/5.2/?.lua"
        "/usr/local/lib/lua/5.2/?/init.lua"
        "./?.lua",
        os.environ.get("LUA_PATH", "")
    ])
    os.environ["LUA_CPATH"] = ";".join([
        os.path.join(targets_dir, "?.so"),
        os.path.join(install_dir, "lib/lua/5.2/?.so"),
        os.path.join(os.environ["HOME"], ".luarocks/lib/lua/5.2/?.so"),
        "/usr/local/lib/lua/5.2/?.so",
        "/usr/local/lib/lua/5.2/loadall.so",
        "./?.so",
        os.environ.get("LUA_CPATH", "")
    ])

    lua_exec = os.path.join(install_dir, "bin", "lua")
    print lua_exec
    luacov_exec = os.path.join(install_dir, "bin", "luacov")
    analyzecov_exec = os.path.join(this_dir, "analyze_coverage.py")

    if not args.exp_dir:
        os.execvp(lua_exec, [lua_exec, args.test_file])
    else:
        for file_name in [LCOV_OUT, LCOV_REPORT]:
            path = os.path.join(args.exp_dir, file_name)
            if os.path.exists(path):
                os.unlink(path)

        old_dir = os.getcwd()
        os.chdir(args.exp_dir)

        subprocess.check_call([lua_exec, "-lluacov", args.test_file, "hl_test_cases.dat"])
        assert os.path.isfile(LCOV_OUT), "Did not get the coverage file."

        subprocess.check_call([luacov_exec])
        assert os.path.isfile(LCOV_REPORT), "Did not get the coverage report."

        os.chdir(old_dir)

        subprocess.check_call([analyzecov_exec, args.exp_dir])


if __name__ == "__main__":
    main()
