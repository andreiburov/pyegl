from setuptools import setup
from torch.utils.cpp_extension import BuildExtension, CUDAExtension
import os.path as osp

setup(
    name='toy',
    version='0.1',
    author='Andrei Burov',
    ext_modules=[
        CUDAExtension('toy', 
											[osp.join('toy', 'toy.cpp')],
                      cflags=[],
											include_dirs=[],
                      library_dirs=[],
                      libraries=['dl'])
    ],
    data_files=[('shaders', [osp.join('toy', 'shaders', 'vertexShader.glsl')])],
    cmdclass={
        'build_ext': BuildExtension
    })
