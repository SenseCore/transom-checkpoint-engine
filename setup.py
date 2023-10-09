#! /usr/bin/env python
# -*- coding: utf-8 -*-
try:
    from setuptools import find_packages, setup
except ImportError:
    from distutils.core import setup

import setuptools

setup(
    name="transomSnapshot",
    author="",
    version="0.0.1",
    license="MIT",
    description="project describe",
    long_description="""long description""",
    # 包内需要引用的文件夹
    packages=["transomSnapshot/engine", "transomSnapshot/cli"],
    package_data={
        "": [
            "../transom_snapshot_*",
            "*.so",
        ],
    },
    entry_points={
        "console_scripts": [
            "transom_snapshot_cli = transomSnapshot.cli.cli:main",
        ],
    },
    # auto install dep
    install_requires=[
        "loguru",
        "requests",
    ],
    setup_requires=["torch", "deepspeed"],
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Operating System :: Linux",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Topic :: Software Development :: Libraries",
    ],
    python_requires=">=3",
    zip_safe=False,
)
