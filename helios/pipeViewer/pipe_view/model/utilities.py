from __future__ import annotations
from logging import info, debug, error, warning, LogRecord, StrFormatStyle
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
from typing import Any, Dict, Optional, Union, cast
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


class Settings:
    '''
    will convert a nested dict into an object to make it easier to access various members
    '''

    def __init__(self, value: Union[str, Dict[str, Any]]) -> None:
        '''
        take a filename, open it as a yaml file and convert that into an object
        with an arbitrary hierarchy
        '''
        if isinstance(value, str):
            value = yaml.safe_load(open(value))

        assert isinstance(value, dict)

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

    def __init__(self, prefixValue: Optional[str] = None, isSmoke: bool = True, autoWidth: bool = False) -> None:
        self.reset(prefixValue, isSmoke, autoWidth)

    def reset(self, prefixValue: Optional[str] = None, isSmoke: bool = True, autoWidth: bool = False) -> None:

        self.formats: Dict[Union[int, str], StrFormatStyle] = {}

        if autoWidth:
            width = len(prefixValue) if prefixValue else 1
        else:
            width = 16

        for k, v in LogFormatter.baseFormats.items():
            k = cast(Union[int, str], k)
            # TODO find some more elegant way to get this result
            # if k != logging.INFO and prefixValue:
            if prefixValue and (isSmoke or k != logging.INFO):
                self.formats[k] = StrFormatStyle("%s%s" % (("[%%s%%%ds%%s] " % width) % (bcolors.OKGREEN if sys.stdout.isatty() else '',
                                                                                         prefixValue if prefixValue else "",
                                                                                         bcolors.ENDC if sys.stdout.isatty() else ''),
                                                            v))
            else:
                self.formats[k] = StrFormatStyle(v)

    def format(self, record: LogRecord) -> str:
        # This doesn't play nicely with the base definition of logging.Formatter
        self._style = self.formats.get(record.levelno, LogFormatter.baseFormats['DEFAULT']) # type: ignore
        return logging.Formatter.format(self, record)
