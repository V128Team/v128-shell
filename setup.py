#!/usr/bin/python3

from setuptools import setup, find_packages
from os import path
from io import open

here = path.abspath(path.dirname(__file__))

with open(path.join(here, 'README.md'), encoding='utf-8') as f:
    long_description = f.read()

setup(
    name='v128-shell',
    version='1.0.0',
    description='A wlroots-based compositor shell for the v128',
    long_description=long_description,
    long_description_content_type='text/markdown',
    url='https://github.com/v128team/v128-shell',
    author='June Tate-Gans',
    author_email='june@theonelab.com',
    license='GPLv2',
    classifiers = [
        'Development Status :: 4 - Beta',
        'Intended Audience :: End Users',
        'Topic :: Utilities',
        'Licensed :: OSI Approved :: GNU General Public License Version 2',
        'Operating System :: Linux',
        'Programming Language :: Python :: 3',
    ],
    keywords='wayland shell',
    packages=find_packages(),
    python_requires='>=3.5.0',
    install_requires=[
        'pywlroots'
    ],
    data_files=[],
    entry_points={
        'console_scripts': [
            'v128-shell=v128-shell.main:main',
        ],
    },
)
