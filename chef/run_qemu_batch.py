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

"""Run a set of qemu tasks in parallel."""

__author__ = "stefan.bucur@epfl.ch (Stefan Bucur)"


import argparse
import os
import pipes
import shlex
import time
import yaml

THIS_DIR = os.path.dirname(__file__)
CHEF_ROOT = os.path.join(THIS_DIR, "..", "..", "..")
OUTPUT_DIR = os.path.abspath(os.path.join(CHEF_ROOT, "expdata"))
RUN_QEMU_PATH = os.path.join(THIS_DIR, "run_qemu.py")


class ExperimentBuilder(object):
    COMMAND_BASE_PORT = 1234

    def __init__(self):
        self._counter = 0
        self._experiments = []

    def create_experiment(self, name, duration, batch_data, env_vars=None):
        experiment_id = "%s-%s" % (name, time.strftime("%Y-%m-%d-%H-%M-%S"))
        experiment_dir = os.path.join(OUTPUT_DIR, experiment_id)
        os.makedirs(experiment_dir)

        last_dir = os.path.join(OUTPUT_DIR, "last")
        if os.path.islink(last_dir):
            os.unlink(last_dir)
        if not os.path.exists(last_dir):
            os.symlink(experiment_id, last_dir)

        commands_file = os.path.join(experiment_dir, "commands.txt")

        with open(commands_file, "w") as f:
            for job in batch_data["jobs"]:
                for config in batch_data["config"]:
                    client_dir = os.path.join(experiment_dir, config["name"], job["name"])
                    os.makedirs(client_dir)

                    command_line = [RUN_QEMU_PATH, "--batch",
                                    "--command-port", str(self.COMMAND_BASE_PORT + self._counter),
                                    "sym", "--config", str(config["file"]), "--out-dir", client_dir,
                                    "--time-out", str(duration)]
                    if env_vars:
                        for var in env_vars:
                            command_line.extend(["-e", var])
                    command_line.extend(shlex.split(job["command"]))
                    print >>f, "%s 2>&1 | tee %s" % (" ".join(pipes.quote(arg) for arg in command_line),
                                                     os.path.join(client_dir, "output.log"))
                    self._counter += 1
        self._experiments.append(experiment_dir)

    def execute(self, ungroup=False, jobs=None):
        os.execlp("/bin/bash", "/bin/bash", "-c",
                  "cat %(exps)s | parallel --delay 1 %(ungroup)s %(jobs)s --joblog %(joblog)s | tee %(output)s" % {
                      "exps": " ".join([os.path.join(exp_dir, "commands.txt") for exp_dir in self._experiments]),
                      "ungroup": "--ungroup" if ungroup else "",
                      "jobs": ("--j %d" % jobs) if jobs else "",
                      "joblog": os.path.join(OUTPUT_DIR, "last", "joblog.log"),
                      "output": os.path.join(OUTPUT_DIR, "last", "output.log")
                  })


def main():
    parser = argparse.ArgumentParser(
        description="Run a set of qemu tasks in parallel.")
    parser.add_argument("name",
                        help="A tag identifying the batch")
    parser.add_argument("duration", type=int,
                        help="The max duration of each task")
    parser.add_argument("batchfile", type=argparse.FileType("r"),
                        help="The batch configuration")

    parser.add_argument("--ungroup", "-u", action="store_true",
                        help="Do not enforce output grouping (print it interleaved)")
    parser.add_argument("--jobs", "-j", type=int,
                        help="The number of jobs to run at once (default NUM_CPU)")
    parser.add_argument("--env-var", "-e", action="append",
                        help="Environment variable for each command")
    parser.add_argument("-n", "--trials", type=int,
                        help="Number of experiment trials")

    args = parser.parse_args()
    batch_data = yaml.load(args.batchfile)

    exp_builder = ExperimentBuilder()
    if args.trials:
        for i in range(1, args.trials+1):
            exp_builder.create_experiment("%s-%d" % (args.name, i), args.duration, batch_data, env_vars=args.env_var)
    else:
        exp_builder.create_experiment(args.name, args.duration, batch_data, env_vars=args.env_var)

    exp_builder.execute(args.ungroup, args.jobs)


if __name__ == "__main__":
    main()
