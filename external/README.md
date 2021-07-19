HTM Core external libraries
=============================


HTM Core depends on a number of external libraries. The download and build of these libraries is
integrated into the cmake-based build of htm.core.  The code that does this are in external/*.cmake

- Boost.cmake      - If needed, finds the boost installation 1.69.0. Boost needs to be built with -fPIC so cannot use externally installed.
- digestpp.cmake   - Download/install digestpp @ 36fa6ca : Hash digest lib (header only)
- eigen.cmake      - Downloads eigen 3.3.7  (header only)
- gtest.cmake      - Downloads and installs googletest 1.8.1
- mnist_data.cmake - Downloads the mnist data set from repository master.
- pybind11.cmake   - Downloads and installs pybind11 2.2.4  (header only)
- libayml.cmake    - Downloads and installs libyaml which is an alternative to yaml-cpp (default) 
- cpp-httplib.cmake- Downloads and installs cpp-httplib a REST server (header only)

External packages included within this repository are built into the common library:

- MurmurHash3

