from setuptools import setup
from torch.utils.cpp_extension import BuildExtension, CUDAExtension
import os.path as osp

setup(
    name='pyegl',
    version='0.2',
    author='Andrei Burov',
    ext_modules=[
        CUDAExtension('pyegl', [osp.join('pyegl', 'pyegl.cpp'), osp.join('pyegl', 'opengl_helper.cpp'), osp.join('pyegl', 'deps', 'FreeImageHelper.cpp')],
                      include_dirs=[osp.join(osp.dirname(osp.realpath(__file__)), 'deps'), osp.join(osp.dirname(osp.realpath(__file__)), 'deps/glew-2.1.0/include')],
                      library_dirs=[osp.join(osp.dirname(osp.realpath(__file__)), 'deps/glew-2.1.0/lib')],
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
