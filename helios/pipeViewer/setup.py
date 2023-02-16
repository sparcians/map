import setuptools
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
# Transaction DB
transaction_db = setuptools.Extension(
    'pipe_view.transactiondb',
    language='c++',
    sources=['transactiondb/src/transactiondb.pyx'],
    libraries=["sparta", "simdb", "hdf5", "sqlite3"],
    pyrex_gdb = True,
    extra_compile_args = compile_args,
)
ext_modules = [transaction_db]


setuptools.setup(
    packages=['pipe_view'],
    ext_modules = cythonize(ext_modules, language_level=3),
    #package_dir={'': 'src'},
#    package_data={
#        'foo': ['foo_ext.pxd', 'c_foo.pxd']
#    },
    include_package_data=True,
    setup_requires=[
        'cython'
    ],
    tests_require=['pytest'],
    zip_safe=False,
    )
