# Document generation

We use Doxygen to generate the documents. The documentation is generated from the .md files and the comments in the source code that follow the
Doxygen conventions.

Check that Doxygen, and its dependecies, are installed on you computer.
Ensure that doxygen and its dependencies can be started from the command line: That their bin directories are in the PATH environment variable.
Ensure that python, and python module "chardet" is installed.
To generate documentation for pyhton modules we use the helper program doxypypy.py. It currently can not handle new syntax that was added in Python 3.8,
see https://github.com/Feneric/doxypypy/issues/70, so we use a patched version: docs/doxypypy.py.

If you are on Linux or Mac:

```
    cd docs  
    ./generate_docs_for_linux  
```

If you are on Windows:

```
    cd docs  
    .\generate_docs_for_windows  
```

The generated files are under docs/html. The main entry point is docs/html/index.html.
The file docs/Doxyfile_common lists the source direcories/files that are to be processed, under key INPUT.
