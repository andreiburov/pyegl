from setuptools import setup
from torch.utils.cpp_extension import BuildExtension, CUDAExtension
import os.path as osp

setup(
    name='pyegl',
    version='0.1',
    author='Andrei Burov',
    ext_modules=[
        CUDAExtension('pyegl', 
											[osp.join('pyegl', 'pyegl.cpp'), osp.join('pyegl', 'FreeImageHelper.cpp')],
											include_dirs=['/rhome/aburov/.local/include'],
                      library_dirs=['/rhome/aburov/.local/lib', '/rhome/aburov/.local/lib64'],
                      libraries=['dl', 'freeimage', 'GL', 'EGL', 'GLESv2', 'GLEW'])
    ],
    data_files=[('shaders', [
      osp.join('pyegl', 'shaders', 'vertexShader.glsl'),
      osp.join('pyegl', 'shaders', 'geometryShader.glsl'),
      osp.join('pyegl', 'shaders', 'fragmentShader.glsl')
      ])
    ],
    cmdclass={
        'build_ext': BuildExtension
    })
