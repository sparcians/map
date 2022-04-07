#!/bin/env python3

# # @package Setup file for transactiondb Python modules
#  @note This must support being run in parallel with itself given a different
#  build target.

import subprocess
import sys
import os
import glob
import time
from os.path import dirname, join

# Force use of a different Cython
cython_dir = os.environ.get('CYTHON_DIR', None)
if cython_dir:
    print('CYTHON_DIR overridden with "{0}"'.format(cython_dir))
    sys.path.insert(0, cython_dir)

import Cython
from Cython.Build import cythonize
print('Using CYTHON: {0} : {1}'.format(Cython.__path__, Cython.__version__))

from distutils import sysconfig, spawn
from distutils.sysconfig import get_config_vars # for static lib dir
from distutils.core import setup
from Cython.Distutils.extension import Extension
from Cython.Distutils import build_ext

# Environment Setup
plat_flags = os.environ.get('SIM_PLATFLAGS', "-m64")
if 'TARGETDIR' not in os.environ:
    print("must set TARGETDIR in env, not currently set")
    sys.exit(1)
destination_dir = os.environ["TARGETDIR"]
system_include_dirs = [a.strip('\\') for a in os.environ.get("SYSINCDIRS", None).split()]

sparta_base_dir = os.environ["SPARTA_BASE"]
required_sparta_libs = os.environ['REQUIRED_SPARTA_LIBS'].split()
required_sparta_libs = [x.lstrip('-l').strip('\\') for x in required_sparta_libs]
sparta_inc_dir = sparta_base_dir
selected_modules = os.environ.get("BUILD_MODULES", "").split()
py_src_dir = os.environ["PY_SRC_DIR"]

print('\n\nDEST DIR "{}"'.format(destination_dir))

sparta_lib_path = os.environ['LIB_SPARTA_BUILT_PATH'].split()
sparta_lib_path = [x.lstrip('-L').strip('\\') for x in sparta_lib_path]
print(sparta_lib_path, required_sparta_libs)

compile_args = ['-std=c++17'] # Required for sparta C++ code
if plat_flags != '':
    compile_args.extend(plat_flags.split())
def_macros = []
link_args = ['-g3']

build_type = os.environ.get("CMAKE_BUILD_TYPE", "Release")
if(build_type == 'Debug'):
    print('Building in DEBUG mode')
    compile_args.extend(('-g3', '-O0'))
else:
    print('Building in Release mode')
    def_macros.append(('NDEBUG', None))
    compile_args.extend(('-g3', '-Ofast'))


# conda python sysconfig data contains '-Wl,export_dynamic' which the linker isn't using
# causes a warning that gets treated like an error, let's not care about unused linker args
# for right now
#compile_args.extend(('-Wno-unused-command-line-argument',))

if os.environ.get('VERBOSE'):
    compile_args.extend(['-v'])
    link_args.extend([ '-v' ])

# All Modules which can be built
PYTHON_STATIC_LIBS = get_config_vars()['LIBDIR']
MODULES = {'transactiondb' : ('transactiondb.pyx',),
           'argosdboutputter' : ('argosdboutputter.pyx',),
           }

# Select a subset of the modules to build based on command-line options
if selected_modules:
    modules = {}
    for k in MODULES:
        if k in selected_modules:
            modules[k] = MODULES[k]
    if not modules:
        print('BUILD_MODULES was specified, but none of them were modules that ' \
              'this script knows how to build: {0}'.format(selected_modules), file = sys.stderr)
        sys.exit(1)
else:
    modules = MODULES

if not os.path.exists(destination_dir):
    try:
        print('Making directory "{0}"'.format(destination_dir))
        os.makedirs(destination_dir)
    except FileExistsError:
        # if building in parallel then this is a race condition
        # just ignore it, they're all trying to do the same thing
        pass

# Build each module
for module_name, source_files in modules.items():

    out_file = module_name + '.so'

    # Source Files
    sources = [os.path.join(py_src_dir, source_file) for source_file in source_files]

    ext_def = Extension(module_name,
                        sources = sources,
                        include_dirs = [sparta_inc_dir ] + system_include_dirs,
                        libraries = required_sparta_libs,
                        library_dirs = sparta_lib_path,
                        define_macros = def_macros,
                        extra_compile_args = compile_args,
                        extra_link_args = link_args,
                        #cython_directives = {'language_level': 3},
                        language = "c++",
                        pyrex_gdb = True, # Build in
                        )

    # Build
    sysconfig.get_config_vars()['CC'] = 'clang'
    sysconfig.get_config_vars()['CC'] = 'clang++'
    setup(
        cmdclass = {'build_ext': build_ext},
        ext_modules = cythonize([ext_def],
                                compiler_directives = {"language_level" : "3"})
        )

    # Move built files
    # TODO seems to be a race condition where this file is sometimes not there
    for _ in range(25):
        time.sleep(1)
        out_file = glob.glob(f"{module_name}.*.so")
        if out_file:
            print(f"Breaking: found {out_file}")
            break
    if out_file:
        out_file = out_file[0]
    else:
        print(f"Could not find {module_name} to copy, exiting")
        sys.exit(-1)
    moduleName = f"{module_name}.so"
    print(f'Moving built module "{out_file}" to directory "{destination_dir}"')
    os.rename(out_file, os.path.join(destination_dir, out_file))

print(f'Successfully built {len(modules)} python extension modules')
