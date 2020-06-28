from logging import info, debug, error, warning
import logging
import os
from os.path import join, isfile, isdir, isabs, abspath, dirname, realpath
import re
import sys
import shutil
import signal
from subprocess import Popen, PIPE, STDOUT
import subprocess
import time
import yaml


class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'


class Settings():
    '''
    will convert a nested dict into an object to make it easier to access various members
    '''

    def __init__(self, value):
        '''
        take a filename, open it as a yaml file and convert that into an object
        with an arbitrary hierarchy
        '''
        if isinstance(value, str):
            value = yaml.safe_load(open(value))

        for a, b in value.items():
            if isinstance(b, (list, tuple)):
                setattr(self, a, [Settings(x) if isinstance(x, dict) else x for x in b])
            else:
                setattr(self, a, Settings(b) if isinstance(b, dict) else b)


class LogFormatter(logging.Formatter):

    baseFormats = {logging.DEBUG : "DEBUG: {funcName}:{lineno}: {message}",
                   logging.WARNING: "WARN: {message}",
                   logging.ERROR : "ERROR: {funcName}: {message}",
                   logging.INFO : "{message}",
                   'DEFAULT' : "{levelname}: {message}"}

    def __init__(self, prefixValue = None, isSmoke = True, autoWidth = False):
        self.reset(prefixValue, isSmoke, autoWidth)

    def reset(self, prefixValue = None, isSmoke = True, autoWidth = False):

        self.formats = {}

        if autoWidth:
            width = len(prefixValue) if prefixValue else 1
        else:
            width = 16

        for k, v in LogFormatter.baseFormats.items():
            # TODO find some more elegant way to get this result
            # if k != logging.INFO and prefixValue:
            if prefixValue and (isSmoke or k != logging.INFO):
                self.formats[k] = logging._STYLES['{'][0]("%s%s" % (("[%%s%%%ds%%s] " % width) % (bcolors.OKGREEN if sys.stdout.isatty() else '',
                                                                                                  prefixValue if prefixValue else "",
                                                                                                  bcolors.ENDC if sys.stdout.isatty() else ''),
                                                                    v))
            else:
                self.formats[k] = logging._STYLES['{'][0](v)

    def format(self, record):
        self._style = self.formats.get(record.levelno, LogFormatter.baseFormats['DEFAULT'])
        return logging.Formatter.format(self, record)


def clearQueue(queue):
    while not queue.empty():
        queue.get()
        queue.task_done()


def signalHandler(signum, frame, process):
    process.send_signal(signum)
    # process.send_signal(signal.SIGSTOP)


def runWithLog(commandline, env = None, useShell = True, interactive = False, silent = False):
    rc = 0
    try:
        newEnv = os.environ.copy()

        if env:
            for k, v in env.items():
                newEnv[k] = "%s:%s" % (newEnv[k], v) if k in newEnv else v

        process = Popen(commandline,
                        stdout = open(os.devnull, 'w') if silent else PIPE,
                        stderr = STDOUT,
                        shell = useShell,
                        env = newEnv)

        if interactive:
            sigHandler = lambda signum, frame: signalHandler(signum, frame, process)
            signal.signal(signal.SIGINT, sigHandler)

        if not silent:
            with process.stdout:
                if interactive:
                    for char in iter(lambda: process.stdout.read(1), b''):
                        sys.stdout.write(char.decode('utf-8', errors = 'replace'))
                        sys.stdout.flush()
                else:
                    for line in iter(process.stdout.readline, b''):
                        info(line.decode('utf-8', errors = 'replace').strip(os.linesep))
        rc = process.wait()

        if interactive:
            signal.signal(signal.SIGINT, signal.SIG_DFL)
    except subprocess.CalledProcessError as cpe:
        error("command [%s] returned [%s]" % (cpe.cmd, str(cpe.returncode)))
        rc = cpe.returncode
    return rc


class Runner():

    def __init__(self, commandline, name, env = None, useShell = True, interactive = False, silent = False):
        self.commandline = commandline
        self.name = name
        self.env = env
        self.useShell = useShell
        self.interactive = interactive
        self.silent = silent
        self.process = None

    def stop(self, wait = False):
        if self.process:
            debug("killing process {}".format(self.process.pid))
            os.kill(self.process.pid, signal.SIGTERM)

        if wait:
            time.sleep(1)
            self.process.wait()

    def runWithLog(self):
        debug("launching {}".format(self.name))
        rc = 0
        try:
            newEnv = os.environ.copy()

            if self.env:
                for k, v in self.env.items():
                    newEnv[k] = "%s:%s" % (newEnv[k], v) if k in newEnv else v

            self.process = Popen(self.commandline,
                                 stdout = open(os.devnull, 'w') if self.silent else PIPE,
                                 stderr = STDOUT,
                                 shell = self.useShell,
                                 env = newEnv)

            if self.interactive:
                sigHandler = lambda signum, frame: signalHandler(signum, frame, self.process)
                signal.signal(signal.SIGINT, sigHandler)

            if not self.silent:
                with self.process.stdout:
                    if self.interactive:
                        for char in iter(lambda: self.process.stdout.read(1), b''):
                            sys.stdout.write(char.decode('utf-8', errors = 'replace'))
                            sys.stdout.flush()
                    else:
                        for line in iter(self.process.stdout.readline, b''):
                            info(line.decode('utf-8', errors = 'replace').strip(os.linesep))
            rc = self.process.wait()

            self.process = None

            if self.interactive:
                signal.signal(signal.SIGINT, signal.SIG_DFL)
        except subprocess.CalledProcessError as cpe:
            error("command [%s] returned [%s]" % (cpe.cmd, str(cpe.returncode)))
            rc = cpe.returncode
        return rc


def rmdir(directory, forceRm = False):
    '''
    removes everything in the list via nfsrm
    '''
    if not isinstance(directory, list):
        directory = directory.split()
    for item in directory:
        # wait for NFS to catch up, make sure they exist before we delete them...
        for _ in range(4):
            if isfile(item) or isdir(item):
                break
            else:
                time.sleep(1)
    if directory:
        # TODO this is python calling python, make this into a library
        for currentDir in directory:
            if forceRm or runWithLog("python2.7 /home/escher-de/sa/prod/nfsrm %s" % currentDir):
                # a backup way to run this
                shutil.rmtree(currentDir, True)


def checkExecutable(filename):
    if not all([os.path.isfile(filename),
                os.access(filename, os.R_OK),
                os.access(filename, os.X_OK)]):
        raise ValueError("not a valid executable: %s" % abspath(filename))
    else:
        return filename


def checkDirectory(directory):
    if not all([os.path.isdir(directory),
                os.access(directory, os.R_OK)]):
        raise ValueError("not a readable directory: %s" % abspath(directory))
    else:
        return directory


def checkFile(file):
    if not all([os.path.isfile(file),
                os.access(file, os.R_OK)]):
        raise ValueError("not a readable directory: %s" % abspath(file))
    else:
        return file


def isReadableFile(filename):
    '''
    check if this is a file that is readable or not
    '''
    if not os.path.isfile(filename):
        return False
    elif not os.access(filename, os.R_OK):
        raise IOError("%s is not readable" % filename)
    else:
        return True


def ymlConfigs(outputFile):
    '''
    in a certain mode, a file will be generated and several env vars will be set
    '''

    with open(outputFile, 'w+') as tf:
        # with open(sys.argv[1], 'w+') as of:
        cfgs = yaml.load(open('cfgs.yml', 'r'))

        for t, c in cfgs.items():
            var = 'CFG_' + '_'.join('_'.join(t.split('-')).split('.'))
            # cfg = 'export CFG_' + '_'.join('_'.join(t.split('-')).split('.')) + '='
            value = ''
            for (k, v) in c.items():
                # cfg += k + ':' + str(v) + '\\\\n'
                value += r"%s:%d\\n" % (k, v)
            debug("export %s=%s" % (var, value))
            setEnvironment(var, value)
            # of.write(cfg + '\n');
            tf.write(t + '\n');


def makeLink(linkTo, name):

    info("make symlink called %s to %s" % (name, linkTo))
    os.symlink(linkTo, name)


def serializeEnvironment(environ):
    '''
    turn a dict into semicolon-separated list to be passed as a string or written
    '''
    return ';'.join('{}={}'.format(key, value) for key, value in environ.items())


def setEnvironment(key, value, environ = None):
    '''
    set an env var either in os.environ or optionally in a dict passed by the user
    '''
    env = environ if (environ != None) else os.environ

    if key in env:
        warning("overriding %senvironment value for %s which was %s" % ("test " if (environ != None) else "", key, env[key]))
    info("setting %senvironment variable %s = %s" % ("test " if (environ != None) else "", key, value))

    env[key] = value


def addEnvironment(key, value, prepend = False, delimiter = ":", environ = None):
    '''
    append to value in os.environ or optionally to a value in a dict passed by the user
    '''
    env = environ if (environ != None) else os.environ

    if key in env:
        currentValues = env.get(key).split(delimiter)
    else:
        currentValues = []
    value = value.strip()
    if value not in currentValues:
        if prepend:
            currentValues.append(value)
        else:
            currentValues = [value] + currentValues
        joinedValues = delimiter.join(currentValues)
        info("setting %senvironment variable to %s = %s" % ("test " if (environ != None) else "", key, joinedValues))
        env[key] = joinedValues


def checkEnvironment(settings, args):
    '''
    validate that environment vars are set
    TODO set them automatically
    '''
    outputDir = realpath(args.output if isabs(args.output) else abspath(join(os.getcwd(), args.output)))

    while outputDir != "/":
        print(outputDir)
        try:
            st = os.statvfs(outputDir)
            break
        except FileNotFoundError:
            outputDir = dirname(outputDir)

    mbFree = st.f_bavail * st.f_frsize / 1024 / 1024

    if mbFree < 1024:
        raise EnvironmentError("free space less than 1GB (%d), aborting" % mbFree)

    if args.resolution:
        match = re.match("^(\d+)x(\d+)$", args.resolution)
        if match:
            if int(match.groups()[0]) > 4096 or int(match.groups()[1]) > 4096:
                raise ValueError("maximum size of 4096 for any side in resolution, found %s'" % args.resolution)
        else:
            raise ValueError("not able to interpret '%s' as a valid resolution" % args.resolution)

    if "DISPLAY" in os.environ:
        del os.environ["DISPLAY"]

    setEnvironment("DVM_EXE_PATH", dirname(args.dvmExe))

    setEnvironment("MOTRIN_TEST_TIMEOUT", str(settings.general.motrinTestTimeout))

    for var in ['DVM_EXE_PATH', "DVM_EXE_PATH", "CLOG_EXE_PATH"]:
        if var not in os.environ:
            error("%s must be set in the environment for this test to work" % var)
            return False

    for var, file in [("CLOG_EXE_PATH", "clog"),
                      ("CLOG_EXE_PATH", "scoop.py"),
                      ("DVM_EXE_PATH", "dvm")]:
        if var not in os.environ:
            error("%s must be set in order to run regressions" % var)
            return False
        currentPath = join(os.environ[var], file)
        if not isfile(currentPath) and not isdir(currentPath):
            error("$%s/%s -> %s not found" % (var, file, currentPath))
            return False

    if args.realDriverPath:
        libraryList = ["libEGL.so", "libGLESv2.so"]
        for lib in [args.realDriverPath + "/" + s for s in libraryList]:
            if not os.path.exists(lib):
                error("[--real-driver]: could not find %s, check to see that it exists" % lib)
                return False

    # TODO this is certainly not an efficient way to do whatever this is doing
    if "ENABLE_CONFIG" in os.environ:
        ymlConfigs("__cfglist")

    return True
