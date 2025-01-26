# C Project with Makefile

This project provides a comprehensive Makefile for building a C program. It includes multiple build steps, as well as support for creating and linking shared and static libraries. The generated executable can be run using either type of library.

## Project Structure

project
|
+--- bin
|   +--- use-shared-library
|   +--- use-static-library
|
+--- inc
|   +--- hello.h
|
+--- lib
|   +--- hello.h
|   +--- shared
|       +--- libhello.so
|   +--- static
|       +--- libhello.a
|
+--- Makefile
|
+--- obj
|   +--- hellolinux.i
|   +--- hellolinux.o
|   +--- hellolinux.s
|   +--- helloworld.i
|   +--- helloworld.o
|   +--- helloworld.s
|   +--- main.i
|   +--- main.o
|   +--- main.s
|
+--- out
|   +--- output
|
+--- src
|   +--- hellolinux.c
|   +--- helloworld.c
|   +--- main.c
|
+--- README.md

---

## Makefile Targets

### Primary Targets

1. **`make all`**  
   Builds the entire project, including the shared library and a final executable linked with it. The output is located in `out/output`.

2. **`make all_shared`**  
   Builds the project, linking the final executable with the shared library.

3. **`make all_static`**  
   Builds the project, linking the final executable with the static library.

4. **`make run_shared`**  
   Runs the program linked with the shared library. Ensures that the shared library can be found using the `LD_LIBRARY_PATH` environment variable.

5. **`make run_static`**  
   Runs the program linked with the static library. The static library does not require runtime loading.

6. **`make clean`**  
   Cleans all generated files, including object files, libraries, and executables.

## Additional Notes

- The shared library (`libhello.so`) must be included in the `LD_LIBRARY_PATH` when running the program linked with it.
- The static library (`libhello.a`) is linked directly into the executable, so no additional configuration is needed at runtime.

---

## Author

- **Bui Minh Chau**  
  Reach out with any questions or suggestions! 
