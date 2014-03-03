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

"""S2E remote control server."""

__author__ = "stefan.bucur@epfl.ch (Stefan Bucur)"
__version__ = "0.1"


import argparse
import json
import os
import pipes

from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer


DEFAULT_HOST = "0.0.0.0"
DEFAULT_PORT = 4321


class Command(object):
    def __init__(self, args, environment):
        self.args = list(args)
        self.environment = dict(environment)

    def execute(self):
        environ = dict(os.environ)
        environ.update(self.environment)

        os.execvpe(self.args[0], self.args, environ)

    @classmethod
    def from_json(cls, json_str):
        data = json.loads(json_str)
        return Command(data["args"], data["environment"])

    def __str__(self):
        return (" ".join("%s=%s" % (k, pipes.quote(v)) for k, v in self.environment.iteritems())
                + " " + " ".join(pipes.quote(arg) for arg in self.args))


class CommandServer(HTTPServer):
    def __init__(self, *args, **kwargs):
        HTTPServer.__init__(self, *args, **kwargs)
        self.pending_command = None


class CommandHandler(BaseHTTPRequestHandler):
    server_version = "S2ECommandServer/" + __version__

    def do_POST(self):
        if self.path == "/command":
            self.do_command()
        else:
            self.send_error(404)

    def do_command(self):
        cmd_length = int(self.headers.getheader('content-length'))
        cmd_string = self.rfile.read(cmd_length)
        try:
            self.server.pending_command = Command.from_json(cmd_string)
        except ValueError:
            self.log_error("Invalid command received.")
            self.send_error(400)
            return

        self.log_message("Received command: %s" % str(self.server.pending_command))
        self.send_response(200)


def main():
    parser = argparse.ArgumentParser(description="S2E remote control server.")
    parser.add_argument("-p", "--port", type=int, default=DEFAULT_PORT,
                        help="Listening port")
    args = parser.parse_args()

    httpd = CommandServer((DEFAULT_HOST, args.port), CommandHandler)
    while not httpd.pending_command:
        httpd.handle_request()

    # Not sure if this is a biggie for us, but it's worth getting rid of other sockets before exec-ing
    httpd.server_close()

    httpd.pending_command.execute()


if __name__ == "__main__":
    main()
