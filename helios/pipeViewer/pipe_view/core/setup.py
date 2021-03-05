#!/usr/bin/env python3

import subprocess
import sys
import os
import os.path
import glob
import distutils
import shutil

# Force use of a different Cython
cython_dir = os.environ.get('CYTHON_DIR', None)
if cython_dir:
    print('CYTHON_DIR overridden with "{0}"'.format(cython_dir))
    sys.path.insert(0, cython_dir)

import Cython
print('Using CYTHON: {0} : {1}'.format(Cython.__path__, Cython.__version__))

from distutils.sysconfig import get_config_vars # for static lib dir
from distutils.core import setup
from Cython.Distutils.extension import Extension
from Cython.Distutils import build_ext
from Cython.Build import cythonize
from pathlib import Path

import pkgutil
wx_pkg = pkgutil.get_loader('wx')
inc_dirs = [os.path.join(os.path.dirname(wx_pkg.get_filename()), 'include')]

# Environment Setup
if 'TARGETDIR' not in os.environ:
    print("must set TARGETDIR in env, not currently set")
    sys.exit(1)
destination_dir = os.environ["TARGETDIR"]
extension = os.environ.get("BUILD", '') # Required from caller for choosing an extension to build

system_include_dirs = []
# cython likes to do things like #include "string", so this fixes that
if "clang" in os.environ.get("CC",""):
    system_include_dirs.append(os.path.join(os.path.dirname(os.path.dirname(distutils.spawn.find_executable(os.environ["CC"]))), "include", "c++", "v1"))

py_src_dir = Path(__file__).parent.resolve() / 'src'

# Support for systems with GTK3 and GTK2 installed side-by-side

# Check for user-specified wx-config
wx_config = os.environ.get('WX_CONFIG')

# Get canonical path
if wx_config is not None:
    wx_config = os.path.realpath(wx_config)

# Try GTK3-specific utility next
if wx_config is None:
    wx_config = shutil.which('wx-config-gtk3')

# Try generic wx-config next
if wx_config is None:
    wx_config = shutil.which('wx-config')

# Couldn't find wx-config - cannot continue
if wx_config is None:
    print('wx-config must be installed and present in your PATH in order to build Argos')
    sys.exit(1)

flags = subprocess.check_output([wx_config, '--cppflags']).decode('utf-8')
flags = flags.split()
wx_inc_dirs = [flg[2:] for flg in flags if flg.startswith('-I')]
wx_defines = [tuple(flg[2:].split('=')) for flg in flags if flg[:2] == '-D']


def ensure_2_tuple(t):
    if len(t) == 1:
        return t[0], None
    return t


wx_defines = list(map(ensure_2_tuple, wx_defines))

flags = subprocess.check_output([wx_config, '--libs']).decode('utf-8')
flags = flags.split()
wx_link_args = flags

compile_args = ['--std=c++17'] # Required for ISL C++ code

# override strict flags (needed when moving to spa-3 gcc6.3)
compile_args += ['-Wno-cast-qual', '-Wno-deprecated-declarations', '-Wno-strict-aliasing', '-Wall', '-Werror', '-Wpedantic']

def_macros = wx_defines[:] + [('_ARGOS_VERSION', '"\'' + os.environ.get('ARGOS_VERSION', 'unknown') + '\'"')]
inc_dirs += wx_inc_dirs[:]
link_args = wx_link_args[:] # Building statically does not work in RHEL6.5 at this point # + ['-static', '-fPIC']


dbg_build = os.environ.get("DBG_BUILD", 0)
debug = False
if(dbg_build in (1, '1', 'true', 'True', 'TRUE')):
    print('Building in DEBUG mode')
    compile_args.extend(('-g3', '-O0'))
    debug = True
else:
    print('Building in Release mode')
    def_macros.append(('NDEBUG',))
    compile_args.extend(('-g3', '-O3'))

# conda python sysconfig data contains '-Wl,export_dynamic' which the linker isn't using
# causes a LLVM warning that gets treated like an error, let's not care about unused linker args
# for right now
if "clang" in os.environ.get("CC",""):
    compile_args.extend(('-Wno-unused-command-line-argument',))

for d in system_include_dirs:
    compile_args.append('-isystem' + d)

# Modules to build
PYTHON_STATIC_LIBS = get_config_vars()['LIBDIR']
MODULES = {'logsearch' : {'sources': ('logsearch.pyx', 'log_search.cpp',)},
           'core' : {'sources': ('core.pyx',)},
           }


if False:
    # Select the module from the environment variable
    if extension.strip():
        modules = MODULES
    else:
        if extension not in MODULES:
            raise Exception(f'Extension "{extension}" specified by env var BUILD was not found in known MODULES available for building')
        modules = {extension: MODULES[extension]}
else:
    modules = MODULES

me = "pipe_view/core/setup.py: "
print(me, "def_macros: ", def_macros)
print(me, "inc_dirs:   ", inc_dirs)
print(me, "link_args:  ", link_args)
'''
print(('Making directory "{0}"'.format(destination_dir)))
try:
    if destination_dir and not os.path.exists(destination_dir):
        os.makedirs(destination_dir)
except FileExistsError:
    pass
'''
# Build each module
for module_name, module_info in modules.items():

    if isinstance(module_info, dict):
        source_files = module_info['sources'] # Required
    else:
        source_files = module_info

    # Source Files
    sources = [os.path.join(py_src_dir, source_file) for source_file in source_files]

    ext_def = Extension(module_name,
                        sources,
                        include_dirs = inc_dirs,
                        define_macros = def_macros,
                        extra_compile_args = compile_args,
                        extra_link_args = link_args,
                        language = 'c++',
                        )

    # Build 
    print((me, "CC orig: ", os.environ.get('CC', '""')))
    print((me, "CXX orig: ", os.environ.get('CXX', '""')))
    print((me, "CFLAGS: ", os.environ.get('CFLAGS', '""')))
    print((me, "CPPFLAGS: ", os.environ.get('CPPFLAGS', '""')))
    setup(cmdclass = {'build_ext': build_ext},
          ext_modules = cythonize([ext_def],
                                  language_level = 3,
                                  gdb_debug = debug))

    # Move built files
    # FIXME instead of doing this, we should correctly move the python files into the build
    # directory so that we're really doing an out-of-source build, but whatever.
    print('')
    out_file = glob.glob("{0}*.so".format(module_name))[0]
    moduleName = "{}.so".format(module_name)
    print(('Moving built module "{0}" to directory "{1}" as "{2}"'.format(out_file, destination_dir, moduleName)))
    os.rename(out_file, os.path.join(destination_dir, moduleName))

print('Successfully built {0} python extension modules\n'.format(len(modules)))
