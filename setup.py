"""Build the Python binding and its own isolated CPU core with CMake."""
from pathlib import Path
import subprocess
import sys

import pybind11
from setuptools import setup
from setuptools.command.build_ext import build_ext
from setuptools.extension import Extension


class BuildCore(build_ext):
    def build_extensions(self):
        root = Path(__file__).resolve().parent
        core = Path(self.build_temp).resolve() / 'core'
        subprocess.check_call([
            'cmake', '-S', str(root), '-B', str(core),
            '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=OFF',
            '-DMARKOV_CERO_ENABLE_CUDA=OFF',
        ])
        subprocess.check_call([
            'cmake', '--build', str(core), '--config', 'Release',
            '--target', 'markov_cero_core', '--parallel', '2',
        ])
        candidates = [core / 'libmarkov_cero_core.a',
                      core / 'Release' / 'markov_cero_core.lib',
                      core / 'markov_cero_core.lib']
        library = next((path for path in candidates if path.is_file()), None)
        if library is None:
            raise RuntimeError('CMake did not produce the core static library')
        for extension in self.extensions:
            extension.extra_objects = [str(library)]
        # The core archive is rebuilt independently of binding source timestamps.
        # Always relink so an incremental wheel cannot embed the previous core.
        self.force = True
        super().build_extensions()


windows = sys.platform == 'win32'
extension = Extension(
    'markov_cero._core', sources=[str(path) for path in sorted(Path('python/src').glob('*.cpp'))],
    include_dirs=['include', 'gpu/include', pybind11.get_include()],
    extra_compile_args=['/std:c++20', '/O2'] if windows else
                       ['-std=c++20', '-O2', '-fvisibility=hidden'],
    extra_link_args=[] if windows else ['-pthread'], language='c++',
)

setup(ext_modules=[extension], cmdclass={'build_ext': BuildCore}, zip_safe=False)
