from setuptools import setup
from torch.utils.cpp_extension import BuildExtension, CUDAExtension
import os.path as osp

setup(
    name='pyegl',
    version='0.2',
    author='Andrei Burov',
    ext_modules=[
        CUDAExtension('pyegl', [osp.join('pyegl', 'pyegl.cpp'), osp.join('pyegl', 'deps', 'FreeImageHelper.cpp')],
                      include_dirs=['/rhome/aburov/.local/include'],
                      library_dirs=['/rhome/aburov/.local/lib', '/rhome/aburov/.local/lib64'],
                      libraries=['dl', 'freeimage', 'GL', 'EGL', 'GLESv2', 'GLEW'])
    ],
    data_files=[('shaders', [
      osp.join('pyegl', 'shaders', 'basic.vs'),
      osp.join('pyegl', 'shaders', 'basic.gs'),
      osp.join('pyegl', 'shaders', 'basic.fs')
      ])
    ],
    cmdclass={
        'build_ext': BuildExtension
    })
