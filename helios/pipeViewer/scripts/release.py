#!/usr/bin/env python

## @package release.py
#  @brief Release script for pipeViewer
#  @note This is derived and simplified from fiat's simple_release.py
#

# Nice output utilities
from utils.utils1 import *
from gitwrap import *

import sys

# Check Interpreter Version
assert sys.version_info[0] == 2 and sys.version_info[1] >= 7, \
'Python interpreter {} version ({}) is too old or is Python 3. This version is ' \
'not supported but might still work if you disable this assertion' \
.format(sys.argv[0], sys.version)

import os
import re
import argparse
import logging
import pprint
import time
import subprocess
import stat

t_start = time.clock()

################################################################################
# Utilities
################################################################################

## Validates a X.Y.X version string and exits with error on failure
def validateVersionString(ver):
    if isinstance(ver, str) is False:
        error('version must be a dotted numeral "X.Y.Z" string, is "{0}"' \
              .format(ver))
    ver_tuple = ver.split('.')
    if len(ver_tuple) != 3:
        error('version must be a dotted numeral "X.Y.Z" string with 3 components. "{0}" has {1} ' \
              'components instead' \
              .format(ver, len(ver_tuple)))
    for item in ver_tuple:
        try:
            val = int(item, 10)
        except ValueError:
            error('All items in version must be base-10 integers. Found a non-integer: "{0}"' \
                  .format(item))

## Prints a dictionary with kvpairs on separate lines, sorted by key, with
#  right-aligned keys and colored values.
def printInfoDict(d):
    key_len = 1
    for k in d:
        key_len = max(key_len, len(k))
    arg_fmt = '{{0:>{0}}} : {COLOR_NORMAL}{{1}}{COLOR_NORMAL}' \
              .format(key_len, **globals())
    for k in sorted(d.keys()):
        print(arg_fmt.format(k,d[k]))

################################################################################
# Constants / Static Configuration
################################################################################

SCRIPTNAME = 'simple_release.py' # Name of this file
START_WORKING_DIR = os.path.abspath(os.getcwd()) # Working dir at start of the script

# Path to the dir of this script
START_SCRIPT_DIR = os.path.split(os.path.abspath(os.path.join(os.getcwd(), sys.argv[0])))[0]

# Absolute path of this script
START_SCRIPT = os.path.abspath(os.path.join(os.getcwd(), sys.argv[0]))


# Application information

APP_NAME = 'pipeViewer' # Name of Application. This is optional and becomes part of the release name

# Filename of the built binary within the directory where make will be invoked
# to build the simulator. This name is constructed from compiler and platform
# information.
# The Final positional arg is an optional debug suffix (e.g. '-dbg')
BUILT_BINARY_FILENAME_FORMAT = \
    'bld-{BUILD_KENRNEL}_{BUILD_ARCH}-{BUILD_COMPILER}{0}/{APP_NAME}'

# Platform information

MAKE_CMD = 'make'

# Kernel (should be changed to evaluate `uname --kernel-name`)
BUILD_KENRNEL = 'Linux'

# Architecture (should be read from build configuration if cross compiling)
BUILD_ARCH = 'x86_64'

# Compiler name
BUILD_COMPILER = 'gcc4.7'


# Variable generation formatters

# Name of the release without versioning information
RELEASE_NAME_BASE = '{APP_NAME}'.format(**globals())

# Name of the release
RELEASE_NAME_FORMAT = '{RELEASE_NAME_BASE}-{version}{version_suffix}'

# Tags to be given to each release (or used to retrieve and re-create a release)
GIT_TAG_FORMAT = 'pipeViewer-release-{version}{version_suffix}'

# Regex for parsing a tag
VERSION_TAG_REGEX = 'pipeViewer-release-[0-9\.a-fA-F]*'

# Wrapper script for pipeViewer
ARGOS_WRAPPER_FORMAT = """
#!/bin/sh

export TRANSACTIONDB_MODULE_DIR="{lib_dir}"
export TRANSACTIONSEARCH_PROGRAM="{bin_dir}/transactionsearch"
"{build_dir}/argos_view/pipeViewer.py" $@
"""

################################################################################
# Script Dependencies
################################################################################

# Strict Python version requirement
# (No known dependencies, but this restricts our testing scope)
vinfo = sys.version_info
if vinfo[0] != 2 or vinfo[1] != 7:
    error('{0} must be run with Python 2.7, version is {1} from {2}' \
          .format(SCRIPTNAME, sys.version, sys.executable))

################################################################################
# Argument Parsing
################################################################################

## Configuration of the release
#  @note Parameters with None values are illegal and must be replaced
release_args = {
    'version'               : None,
    'version_suffix'        : '',
    'build_num'             : '0',
    'dest_dir'              : None,
    'release_name'          : None, # Name identifying this release (includes version)
    'release_dir'           : None,
    'build_dir'             : None,
    'bin_dir'               : None,
    'lib_dir'               : None,
    'argos_dir'             : None,
    'targ_dir'              : None, # subdir where build objects land
    'git_tag'               : None,
    'build_make_dir'        : None,
    'make_args'             : [],   # (e.g. '-j8', 'dbg')
    'make_regress_args'     : [],   # (e.g. '-j8', 'regress', 'regress_dbg')
    'built_binary'          : None,
    'built_binary_filename' : None, # Name of built binary
    'sparta_dir'              : None, # Location of sparta repo to use
    'sparta_transactiondb_dir': None, # Dir containing of sparta transactiondb module (derived from sparta_dir)
}

## Completed release actions and whether they have been done or not
#  False or None implies not done
release_actions = {
    'tagged'          : False, # Did this script add (or ensure) tags
    'published'       : False, # Did this script publish
    'bin_file'        : '',    # Binary to run after release
    'built_binaries'  : [],    # Binaries generated by the build stage
    'versions_string' : None   # Versions string of the release
}

# introspect the TARGETDIR from current SPA conda environment
try:
    dflt_targ_dir = subprocess.check_output(
            ['/sarc/spa/tools/scripts/ezmake_pvar', 'TARGETDIR']).strip()
except:
    dflt_targ_dir = 'bld-Linux_x86_64-gcc6.3'
    warning('Defaulting to spa-3 hard coded --targ-dir="{0}"'.format(dflt_targ_dir))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='pipeViewer Simple Release Script\n' \
                                                 'Run as:\n  release.py 1.0.0 /sarc/spa/releases/pipeViewer/ /path/to/sparta',
                                     epilog='Copyright 2019',
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--targ-dir', type=str, default=dflt_targ_dir,
                        help='subdir where objects are built [{0}]'.format(dflt_targ_dir))
    parser.add_argument('version', metavar='version', type=str, nargs=1,
                        help='version number (X.Y.Z)')
    parser.add_argument('dest_dir', metavar='dest_dir', type=str, nargs=1,
                        help='destination directory. A release will be created within this dir')
    parser.add_argument('sparta_dir', metavar='sparta_dir', type=str, nargs=1,
                        help='sparta repository directory. This must be built. The transactiondb '
                        'module and transaction search program will be copied from within this repository')
    parser.add_argument('-j', type=int, nargs='?', default=12, action='store',
                        help='Multithreaded build')

    parser.add_argument('--debug-build', action='store_true',
                        help='Build a debug release')
    parser.add_argument('--verbose', action='store_true',
                        help='Show verbose output')

    args = parser.parse_args()
    print args


    # Turn on logging if requested
    if args.verbose is True:
        logging.basicConfig(stream=sys.stderr, level=logging.DEBUG) # DEBUG includes INFO


################################################################################
# Handle Specified Arguments
################################################################################

    if args.version:
        release_args['version'] = args.version[0]
        validateVersionString(release_args['version'])
    else:
        # Exit with error
        # TODO: read version from file in repository
        parser.error(fmt_error('version must be specified'))

    if args.dest_dir:
        release_args['dest_dir'] = args.dest_dir[0]
    else:
        # Exit with error
        parser.error(fmt_error('dest-dir must be specified'))

    if args.debug_build is True:
        release_args['make_args'].append('dbg')

    if args.j != 0:
        release_args['make_args'].append('-j{0:d}'.format(int(args.j)))
        release_args['make_regress_args'].append('-j{0:d}'.format(int(args.j)))

    if args.sparta_dir:
        release_args['sparta_dir'] = os.path.abspath(args.sparta_dir[0])
    else:
        parser.error(fmt_error('sparta-dir must be specified'))


################################################################################
# Fill Argument Defaults
################################################################################

    if release_args['release_name'] is None:
        release_args['release_name'] = RELEASE_NAME_FORMAT \
                                       .format(**dict(release_args.items() + globals().items()))

    if release_args['release_dir'] is None:
        release_args['release_dir'] = os.path.abspath(os.path.join(release_args['dest_dir'], \
                                                      release_args['release_name']))

    if release_args['build_dir'] is None:
        release_args['build_dir'] = os.path.abspath(os.path.join(release_args['release_dir'], \
                                                    'build'))

    if release_args['bin_dir'] is None:
        release_args['bin_dir'] = os.path.abspath(os.path.join(release_args['release_dir'], \
                                                  'bin'))

    if release_args['lib_dir'] is None:
        release_args['lib_dir'] = os.path.abspath(os.path.join(release_args['release_dir'], \
                                                  'lib'))

    release_args['sparta_transactiondb_dir'] = \
        os.path.abspath(os.path.join(release_args['sparta_dir'],
                                     'python/transactiondb/{0}'.format(args.targ_dir)))

    release_args['sparta_transactionsearch_program'] = \
        os.path.abspath(os.path.join(release_args['sparta_dir'],
                                     'tools/transactionsearch/{0}/transactionsearch'.format(args.targ_dir)))

    # used by build()
    release_args['targ_dir'] = args.targ_dir

    release_args['git_tag'] = GIT_TAG_FORMAT.format(**dict(release_args.items() + globals().items()))

    ##release_args['built_binary'] = os.path.join(release_args['build_make_dir'],
    ##                               BUILT_BINARY_FILENAME_FORMAT \
    ##                               .format(output_debug_suffix, **globals()))
    ##release_args['built_binary'] = os.path.abspath(release_args['built_binary'])
    ##release_args['built_binary_filename'] = os.path.split(release_args['built_binary'])[1]


################################################################################
# Checkout
################################################################################

def checkout():
    print('\n1. {STEP_COLOR}Fresh Checkout{COLOR_NORMAL}' \
          .format(**globals()))

    # Get the base dir of this repo
    repo_base_dir = getRepoBaseDir()

    versions_string = getVersion(version_tag_regex=VERSION_TAG_REGEX,
                                 hash_size=9)

    print 'Attempting to checkout with version: '
    print versions_string
    release_actions['versions_string'] = versions_string

    if len(versions_string) > 1:
        error('Current repository has the following extra attributes {} which means it cannot be ' \
              'used to make a release. Commit and push all changes before attempting to release.' \
              .format(versions_string[1:]))

    version_info = versions_string
    clone_dir = release_args['build_dir']

    # Clone pipeViewer repo into destination build directory

    start_dir = os.path.abspath(os.getcwd())
    destination = os.path.abspath(clone_dir) # Must be absolute

    # Ensure clone dir does not exist
    if os.path.exists(destination):
        error('Destination "{0}" already exists. Specify a non-exisiting directory where a fresh ' \
              'clone will be created' \
              .format(destination))

    # Ensure we are not already in a repo dir by walking the path from end to start
    dest_path = os.path.split(destination)[0]
    while dest_path != "":
        if os.path.exists(dest_path) is False:
            break

        showShellCd(dest_path)
        if inRepo():
            # In a directory controlled by git (or GIT_DIR is defined)
            error('New clone destination "{0}" is already within another git repository or GIT_DIR ' \
                  'is defined.\nThis is really not a safe thing to do' \
                  .format(destination))

        # Continue chopping path until we get somewhere that exists
        dest_path = os.path.split(dest_path)[0]

    # Ensure that destination can be created by git
    shell_cmd(['mkdir', '-p', os.path.split(destination)[0]])

    # Clone repo (and its consituents)
    print('Cloning into {0} ...' \
          .format(destination))
    shell_cmd([GIT_CMD,
               'clone',
               GIT_REPO_FMT.format('transaction_viewer'),
               destination])

    repo_dir = destination

    # Checkout
    # Check out a clean, identical repository with the same git sha

    start_dir = os.path.abspath(os.getcwd())
    repo_dir = os.path.abspath(repo_dir) # Must be absolute

    # Make sure repo_dir exists
    if os.path.exists(repo_dir) is False:
        error('Destination "{0}" is expected to already exist and be a valid git repo' \
              .format(destination))

    repo_name = 'pipeViewer'

    # Determine tag (or sha) for this repo
    ver = version_info[0]

    showShellCd(destination)

    # Attempt to checkout the tag generated from the version
    print('Checking out tag/sha "{0}"...' \
          .format(ver))

    try:
        cmd = [GIT_CMD,
               "checkout",
               str(ver)]
        print('{EXTOUT_COLOR}{0}{COLOR_NORMAL}'.format(' '.join(str(arg) for arg in cmd), **globals()))
        git_output = subprocess.check_output(cmd,
                                             stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as ex:
        # This is not an error and is expected
        msg = 'Unable to find tag or hash "{0}" in {1}.\n' \
              'Clone at {2} is now in an unstable state and will have to be erased to ' \
              'reclone: {3}' \
              .format(ver, repo_name, repo_dir, ex.output)
        dirty = version_info[1] if len(version_info) > 1 else False
        unpushed = version_info[2] if len(version_info) > 2 else False
        if dirty:
            msg += '\nThis version string came from a dirty repository, so it may have never ' \
                   'been comitted and pushed to the shared repository'
        if unpushed:
            msg += '\nThis version string came from a repository with unpushed changes, so it' \
                   ' may have never been pushed to the shared repository'

        error(msg)

    print('{HAPPY_COLOR}==> New repository checked out to desired versions at {0} {COLOR_NORMAL}' \
          .format(destination, **globals()))


    # Return home
    os.chdir(START_WORKING_DIR)


################################################################################
# Tag
################################################################################
def tag():
    print('\n2. {STEP_COLOR}Tag{COLOR_NORMAL}' \
          .format(**globals()))

    # Directories which must be tagged
    tag_dirs = [release_args['build_dir'],
                ]

    errors = {}
    for td in tag_dirs:
        showShellCd(td)

        if hasGitTag(release_args['git_tag']):
            error('Repository at "{0}" already has tag "{git_tag}"' \
                  .format(td, **release_args))

        try:
            addGitTag(release_args['git_tag'])
        except subprocess.CalledProcessError as ex:
            alert('Failed to tag "{0}". Command exited with error code {1}. Output:' \
                  '{EXTOUT_COLOR}\n{2}\n{COLOR_NORMAL}' \
                  .format(td, ex.returncode, ex.output, **globals()))
            errors[td] = ex

    if len(errors) > 0:
        warning('Errors tagging {0} repositories:\n  {1}' \
                '\nSee output above\nPlease add or move these tags manually'. \
                format('\n  '.join(errors.keys()), len(errors)))

    release_actions['tagged'] = True

    # Return home
    os.chdir(START_WORKING_DIR)


################################################################################
# Build - Mostly application-specific
################################################################################
def build():
    print('\n3. {STEP_COLOR}Build{COLOR_NORMAL}' \
          .format(**globals()))

    clone_dir = release_args['build_dir']
    destination = os.path.abspath(clone_dir) # Must be absolute

    # Ensure clone dir exists
    if not os.path.exists(destination):
        error('Destination "{0}" must exist to make'.format(destination))

    for debug_mode in (False, True):
        build_dir = os.path.abspath(os.path.join(destination, '.'))

        # CD into build directory
        showShellCd(build_dir)

        # Make
        if debug_mode:
            shell_cmd([MAKE_CMD, '-C', build_dir, '-j8', 'DBG_BUILD=1'])
            BUILT_BINARY_FILENAME = 'argos_view/core/{0}/core.so'.format(release_args['targ_dir'])
        else:
            shell_cmd([MAKE_CMD, '-C', build_dir, '-j8'])
            BUILT_BINARY_FILENAME = 'argos_view/core/{0}/core.so'.format(release_args['targ_dir'])

        # Test for resulting binary
        binary_file = os.path.join(build_dir, BUILT_BINARY_FILENAME)
        if os.path.exists(binary_file) is False:
            error('Failed to make binary: "{0}".\nPerhaps make generated a different file? If so, ' \
                  '{START_SCRIPT} must be fixed'.format(binary_file, **globals()))

        print('{HAPPY_COLOR}==> Make Succeeded. Built: {0}{COLOR_NORMAL}' \
              .format(binary_file, **globals()))

    # Return home
    os.chdir(START_WORKING_DIR)


################################################################################
# Publish
################################################################################
def publish():
    print('\n4. {STEP_COLOR}Publish{COLOR_NORMAL}' \
          .format(**globals()))

    bin_file_permissions = stat.S_IRUSR|stat.S_IRGRP|stat.S_IROTH| \
                           stat.S_IWGRP|stat.S_IWUSR| \
                           stat.S_IXUSR|stat.S_IXGRP|stat.S_IXOTH

    # Create a lib dir
    shell_cmd(['mkdir', '-p', release_args['lib_dir']])

    # Create a bin dir
    shell_cmd(['mkdir', '-p', release_args['bin_dir']])

    # Ensure no one else writes to the release dir
    shell_cmd(['chmod',
               '-R',
               'u+w,o-w,g-w',
               release_args['build_dir'],
               ])

    # Create wrapper for pipeViewer
    argos_wrapper = os.path.join(release_args['bin_dir'], 'pipeViewer')
    with open(argos_wrapper, 'w') as f:
        f.write(ARGOS_WRAPPER_FORMAT.format(**release_args))
    release_actions['bin_file'] = argos_wrapper

    os.chmod(argos_wrapper, bin_file_permissions)

    # Copy transaction db module from sparta
    shell_cmd(['cp',
              os.path.join(release_args['sparta_transactiondb_dir'], 'transactiondb.so'),
              os.path.join(release_args['sparta_transactiondb_dir'], 'transactiondb2.so'),
              release_args['lib_dir'] + os.sep])

   # Copy transaction search program from sparta
    shell_cmd(['cp',
              release_args['sparta_transactionsearch_program'],
              release_args['bin_dir'] + os.sep])

    os.chmod(argos_wrapper, bin_file_permissions)

    # Done
    release_actions['published'] = True

    # Return home
    os.chdir(START_WORKING_DIR)


################################################################################
# Show configuration and run
################################################################################

if __name__ == '__main__':
    print('\n0. {STEP_COLOR}Configuration{COLOR_NORMAL}' \
          .format(**globals()))
    print('Release script starting with the following options:')
    printInfoDict(release_args)
    print('')

    try:

        # Normal release path
        checkout()
        tag()
        build()
        publish()

    except KeyboardInterrupt as ex:
        error('Release script aborted by user!\nIt is recommended you delete the repository at {0}.\n' \
              'Alternatively, the --skip-***** options can be '\
              'carefully used to skip any previously completed steps. Use at your own risk, however.' \
              .format(release_args['release_dir']))


################################################################################
# Summary
################################################################################

    print('\n5. {STEP_COLOR}Summary{COLOR_NORMAL}' \
          .format(**globals()))

    t_delta = time.clock() - t_start
    print('\n{HAPPY_COLOR}==> Automated portion of release succeeded in {0:d}m {1:d}s{COLOR_NORMAL}' \
          .format(int(t_delta//60), int(divmod(t_delta, 60)[1]), **globals()))
    print('\n{HAPPY_COLOR}==> Release can be found in "{0}"{COLOR_NORMAL}' \
          .format(release_args['release_dir'], **globals()))
    print('\n{HAPPY_COLOR}==> The version string is "{0}"{COLOR_NORMAL}' \
          .format(release_actions['versions_string'], **globals()))

    if args.verbose:
        printInfoDict(release_actions)
        print('')

    next_todo_num = 1
    print('\n{WARNING_COLOR}ALMOST DONE! NOW YOU MUST DO A FEW LAST THINGS MANUALLY...{COLOR_NORMAL}'.format(**globals()))
    print(' {next_todo_num:>2d}. Test run {}'.format(release_actions['bin_file'], **globals()))
    next_todo_num+=1
    if release_actions['published'] is True:
        print(' {next_todo_num:>2d}. Push the tags added to pipeViewer using "git push --tags" within {0}' \
              .format(release_args['build_dir'], **globals()))
        next_todo_num+=1
    print(' {next_todo_num:>2d}. Make the build directory read-only"' \
          .format(**globals()))
    next_todo_num+=1
    print(' {next_todo_num:>2d}. Uddate the "pipeViewer" script in the tools_scripts repository to point to ' \
          'pipeViewer {} and push' \
          .format(release_args['version'], **globals()))
    next_todo_num+=1
    print(' {next_todo_num:>2d}. Party!' \
          .format(**globals()))
    next_todo_num+=1

    print('') # Extra space

    sys.exit(0)
