#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""The setup script."""

from setuptools import setup, find_packages

with open('README.rst') as readme_file:
    readme = readme_file.read()

with open('HISTORY.rst') as history_file:
    history = history_file.read()

requirements = [ ]

setup_requirements = [ ]

test_requirements = [ ]

from distutils.extension import Extension

module1 = Extension('bprof._bprof',
                    sources = ['src/demo.cpp'],
                    extra_compile_args=['-std=c++17'],
                    language = 'c++17')

setup(
    author="Joel Frederico",
    author_email='joelfrederico@gmail.com',
    python_requires='>=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*, !=3.4.*',
    classifiers=[
        'Development Status :: 2 - Pre-Alpha',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Natural Language :: English',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.5',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
    ],
    description="A Better PROFiler",
    entry_points={
        'console_scripts': [
            'bprof=bprof.cli:main',
        ],
    },
    install_requires=requirements,
    license="MIT license",
    long_description=readme + '\n\n' + history,
    include_package_data=True,
    keywords='bprof',
    name='bprof',
    packages=find_packages(include=['bprof', 'bprof.*']),
    setup_requires=setup_requirements,
    test_suite='tests',
    tests_require=test_requirements,
    url='https://github.com/joelfrederico/bprof',
    version='0.1.2',
    zip_safe=False,
    ext_modules = [module1]
)
