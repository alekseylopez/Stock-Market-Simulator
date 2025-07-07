import os
from pybind11.setup_helpers import Pybind11Extension
from pybind11 import get_cmake_dir
from setuptools import setup, find_packages

# extension module definition
ext_modules = [
    Pybind11Extension(
        "simulator_core",
        [
            "src/cpp/bindings/python_bindings.cpp",
            "src/cpp/core/portfolio.cpp",
            "src/cpp/core/order_book.cpp",
            "src/cpp/core/market_data_engine.cpp",
        ],
        include_dirs=[
            get_cmake_dir()
        ],
        cxx_std=17,
        language="c++",
    )
]

setup(
    name="stock-market-simulator",
    author="Aleksey Lopez",
    author_email="lopezag@umich.edu",
    description="A high-performance stock market simulator",
    long_description=open("README.md").read() if os.path.exists("README.md") else "",
    long_description_content_type="text/markdown",
    ext_modules=ext_modules,
    packages=find_packages(where="src/python"),
    package_dir={"": "src/python"},
    python_requires=">=3.8",
    install_requires=[
        "numpy>=1.20.0",
        "pandas>=1.3.0",
        "matplotlib>=3.4.0",
        "seaborn>=0.11.0",
        "scipy>=1.7.0",
        "pybind11>=2.10.0",
    ]
)