import setuptools
from setuptools import find_packages
from Cython.Build import cythonize
from Cython.Distutils import build_ext

compile_args = ['--std=c++17']  # Required for SPARTA
# override strict flags
compile_args += [
    '-Wno-cast-qual',
    '-Wno-deprecated-declarations',
    '-Wno-strict-aliasing',
    '-Wall',
    '-Wpedantic'
]

# Wrappers for the parts written in Cython
transaction_db = setuptools.Extension(
    'pipe_view.transactiondb',
    language='c++',
    sources=['pipe_view/transactiondb/src/transactiondb.pyx'],
    libraries=["sparta", "hdf5", "sqlite3"],
    pyrex_gdb = True,
    extra_compile_args = compile_args,
)
core = setuptools.Extension(
    'pipe_view.core',
    language='c++',
    sources=['pipe_view/core/src/core.pyx'],
    libraries=["sparta", "hdf5", "sqlite3"],
    pyrex_gdb = True,
    extra_compile_args = compile_args,
)
logsearch = setuptools.Extension(
    'pipe_view.logsearch',
    language='c++',
    sources=['pipe_view/logsearch/src/logsearch.pyx', 'pipe_view/logsearch/src/log_search.cpp'],
    libraries=["sparta", "hdf5", "sqlite3"],
    pyrex_gdb = True,
    extra_compile_args = compile_args,
)
ext_modules = [transaction_db, core, logsearch]


py_packages = ["pipe_view", "pipe_view.misc", "pipe_view.gui",
            "pipe_view.gui.dialogs", "pipe_view.gui.widgets",
            "pipe_view.model", ]

setuptools.setup(
    name='pipe_view',
    packages=py_packages,
    ext_modules = cythonize(ext_modules, language_level=3,
                            include_path=['pipe_view/core/src'], # to find common.pxd
                            ),
    entry_points = {
        'gui_scripts': ['argos=pipe_view.argos:main'],
    },
    package_data={'pipe_view': ['core/src/common.pxd', 'transactiondb/src/common.pxd', 'resources/*.png', 'stubs/*.pyi']},
    include_package_data=True,
    setup_requires=[
        'cython',
        'wxPython',
    ],
    install_requires=[
        'wxPython',
    ],
    tests_require=['pytest'],
    zip_safe=False,
    )
