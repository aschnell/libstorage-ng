#!/usr/bin/python3

from sys import argv, exit
from getopt import getopt, GetoptError
from subprocess import Popen, PIPE
from storage import Environment, Storage, ProbeMode_STANDARD, ProbeMode_STANDARD_WRITE_MOCKUP, TargetMode_DIRECT
from storage import RemoteCallbacksV2, RemoteCommand, RemoteFile, set_remote_callbacks
from storage import set_logger, get_logfile_logger, get_stdout_logger


host = "localhost"
port = "22"
save_mockup = False
save_devicegraph = False


def run_command(name):

    cmd = "ssh -l root %s -p %s %s" % (host, port, name)

    p = Popen(cmd, shell = True, stdout = PIPE, stderr = PIPE, close_fds = True)
    stdout, stderr = p.communicate()

    stdout = stdout.rstrip()
    stderr = stderr.rstrip()

    ret = RemoteCommand()

    # TODO rethink difference between no line and single empty line

    if stdout:
        stdout = stdout.decode("utf-8")
        for line in stdout.split('\n'):
            ret.stdout.push_back(line)

    if stderr:
        stderr = stderr.decode("utf-8")
        for line in stderr.split('\n'):
            ret.stderr.push_back(line)

    ret.exit_code = p.returncode

    return ret


class MyRemoteCallbacks(RemoteCallbacksV2):

    def __init__(self):
        super(MyRemoteCallbacks, self).__init__()

    def get_command(self, name):
        print("command '%s'" % name)
        command = run_command(name)
        return command

    def get_command_v2(self, args):
        name = ""
        for arg in args:
            name = name + arg + " "
        print("command '%s'" % name)
        command = run_command(name)
        return command

    def get_file(self, name):
        print("file '%s'" % name)
        command = run_command("cat '%s'" % name)
        file = RemoteFile(command.stdout)
        return file


def doit():

    set_logger(get_logfile_logger())

    my_remote_callbacks = MyRemoteCallbacks()

    set_remote_callbacks(my_remote_callbacks)

    environment = Environment(True, ProbeMode_STANDARD_WRITE_MOCKUP if save_mockup else ProbeMode_STANDARD, TargetMode_DIRECT)
    if save_mockup:
        environment.set_mockup_filename("mockup.xml")

    storage = Storage(environment)

    try:
        storage.probe()
    except Exception as exception:
        print(exception.what())
        exit(1)

    print()

    probed = storage.get_probed()

    print(probed)

    if save_devicegraph:
        probed.save("devicegraph.xml")


def usage():
    print("usage: remote-probe.py [--host=name] [--port=part] [--save-mockup] [--save-devicegraph]")
    exit(1)

try:
    opts, args = getopt(argv[1:], "", ["host=", "port=", "save-mockup", "save-devicegraph"])

except GetoptError:
    usage()

if len(args) > 0:
    usage()

for o, a in opts:
    if o == "--host":
        host = a
    if o == "--port":
        port = a
    if o == "--save-mockup":
        save_mockup = True
    if o == "--save-devicegraph":
        save_devicegraph = True

doit()
