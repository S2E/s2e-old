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

"""Wrapper script for running S2E by following the RAW/S2E image manipulation workflow."""

__author__ = "stefan.bucur@epfl.ch (Stefan Bucur)"


import argparse
import httplib
import json
import multiprocessing
import os
import signal
import socket
import sys
import pipes
import time

from datetime import datetime, timedelta

THIS_DIR = os.path.dirname(__file__)
S2E_ROOT = os.path.abspath(os.path.join(THIS_DIR, "..", ".."))
CHEF_ROOT = os.path.abspath(os.path.join(S2E_ROOT, ".."))

RAW_IMAGE_PATH = os.path.join(CHEF_ROOT, "vm", "chef_disk.raw")
S2E_IMAGE_PATH = os.path.join(CHEF_ROOT, "vm", "chef_disk.s2e")

DEFAULT_CONFIG_FILE = os.path.join(THIS_DIR, "config", "default-config.lua")

DEFAULT_COMMAND_PORT = 1234
GDB_BIN = "gdb"
COMMAND_SEND_TIMEOUT = 60


class Command(object):
    def __init__(self, args, environment):
        self.args = args
        self.environment = environment

    def to_json(self):
        return json.dumps({
            "args": list(self.args),
            "environment": dict(self.environment)
        })

    @classmethod
    def from_cmd_args(cls, command, environ):
        return Command(list(command),
                       dict(env_var.split("=", 1) for env_var in environ))


class CommandError(Exception):
    pass


def send_command(command, host, port):
    conn = None
    try:
        conn = httplib.HTTPConnection(host, port=port, timeout=COMMAND_SEND_TIMEOUT)
        conn.request("POST", "/command", command.to_json())
        response = conn.getresponse()
        if response.status != httplib.OK:
            raise CommandError("Invalid HTTP response received: %d" % response.status)
    except (socket.error, httplib.HTTPException) as e:
        raise CommandError(e)
    finally:
        if conn:
            conn.close()


def async_send_command(command, host, port, timeout=COMMAND_SEND_TIMEOUT):
    pid = os.getpid()
    if os.fork() != 0:
        return
    # Avoid the pesky KeyboardInterrupts in the child
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    command_deadline = datetime.now() + timedelta(seconds=timeout)
    while True:
        time.sleep(1)
        try:
            os.kill(pid, 0)
        except OSError:
            break
        now = datetime.now()
        if now < command_deadline:
            try:
                send_command(command, host, port)
            except CommandError as e:
                print >>sys.stderr, "** Could not send command (%s). Retrying for %d more seconds." % (
                    e, (command_deadline - now).seconds)
            else:
                break
        else:
            print >>sys.stderr, "** Command timeout. Aborting."
            break
    exit(0)


def kill_me_later(timeout, extra_time=60):
    pid = os.getpid()
    if os.fork() != 0:
        return
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    int_deadline = datetime.now() + timedelta(seconds=timeout)
    kill_deadline = int_deadline + timedelta(seconds=extra_time)
    int_sent = False
    while True:
        time.sleep(1)
        now = datetime.now()
        try:
            if now < int_deadline:
                os.kill(pid, 0)  # Just poll the process
            elif now < kill_deadline:
                os.kill(pid, signal.SIGINT if not int_sent else 0)
                int_sent = True
            else:
                os.kill(pid, signal.SIGKILL)
                break
        except OSError:  # The process terminated
            break
    exit(0)


def parse_cmd_line():
    parser = argparse.ArgumentParser(description="High-level interface to S2E.")
    parser.add_argument("-b", "--batch", action="store_true", default=False,
                        help="Headless (no GUI) mode")
    parser.add_argument("-d", "--debug", action="store_true", default=False,
                        help="Run in debug mode")
    parser.add_argument("-p", "--command-port", type=int, default=DEFAULT_COMMAND_PORT,
                        help="The command port configured for port forwarding")
    parser.add_argument("--gdb", action="store_true", default=False,
                        help="Run under gdb (implies debug mode)")

    modes = parser.add_subparsers(help="The S2E operation mode")

    kvm_mode = modes.add_parser("kvm", help="KVM mode")
    kvm_mode.add_argument("--cores", type=int, default=multiprocessing.cpu_count(),
                          help="Number of virtual cores")
    kvm_mode.set_defaults(mode="kvm")

    prepare_mode = modes.add_parser("prep", help="Prepare mode")
    prepare_mode.add_argument("-l", "--load", action="store_true", default=False,
                              help="Load from snapshot 1")
    prepare_mode.set_defaults(mode="prep")

    symbolic_mode = modes.add_parser("sym", help="Symbolic mode")
    symbolic_mode.add_argument("-f", "--config", default=DEFAULT_CONFIG_FILE,
                               help="The S2E configuration file")
    symbolic_mode.add_argument("-o", "--out-dir",
                               help="S2E output directory")
    symbolic_mode.add_argument("-t", "--time-out", type=int,
                               help="Timeout (in seconds)")
    symbolic_mode.add_argument("-e", "--env-var", action="append",
                               help="Environment variable for the command (can be used multiple times)")
    symbolic_mode.add_argument("command", nargs=argparse.REMAINDER,
                               help="The command to execute")
    symbolic_mode.set_defaults(mode="sym")

    return parser.parse_args()


def build_qemu_cmd_line(args):
    # Construct the qemu path
    qemu_path = os.path.join(S2E_ROOT, "build",
                             ("qemu-release", "qemu-debug")[args.debug or args.gdb],
                             ("i386-softmmu", "i386-s2e-softmmu")[args.mode == "sym"],
                             "qemu-system-i386")

    # Construct the command line
    cmd_line = []
    if args.gdb:
        cmd_line.extend([GDB_BIN, "--args"])
    cmd_line.extend([qemu_path, (S2E_IMAGE_PATH, RAW_IMAGE_PATH)[args.mode == "kvm"],
                     "-cpu", "pentium",
                     "-net", "nic,model=pcnet",
                     "-net", "user,hostfwd=tcp::%d-:4321" % args.command_port,
                     "-serial", "stdio"])

    if args.batch:
        cmd_line.extend(["-vnc", "none", "-monitor", "/dev/null"])

    if args.mode == "kvm":
        cmd_line.extend(["-enable-kvm", "-smp", str(args.cores)])
    if (args.mode == "prep" and args.load) or args.mode == "sym":
        cmd_line.extend(["-loadvm", "1"])
    if args.mode == "sym":
        cmd_line.extend(["-s2e-config-file", args.config,
                         "-s2e-verbose"])
        if args.out_dir:
            cmd_line.extend(["-s2e-output-dir", args.out_dir])

    return cmd_line


def main():
    args = parse_cmd_line()
    qemu_cmd_line = build_qemu_cmd_line(args)

    print >>sys.stderr, "** Executing", " ".join(pipes.quote(arg) for arg in qemu_cmd_line)
    print >>sys.stderr

    if args.mode == "sym":
        if args.time_out:
            kill_me_later(args.time_out)
        if args.command:
            async_send_command(Command.from_cmd_args(args.command, args.env_var or []),
                               "localhost", args.command_port)

    os.execvp(qemu_cmd_line[0], qemu_cmd_line)


if __name__ == "__main__":
    main()
