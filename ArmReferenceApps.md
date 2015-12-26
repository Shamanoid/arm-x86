How to create reference ARM executables (For project contributors only)

# Introduction #

This is a short guide to how ARM reference executables should be created and organized in the code tree.

# Details #

Reference ARM binaries should be created in the ref directory. For each new example application, a numbered folder should be created which is roughly indicative of the time line of the creation of the applications, and indirectly representative of the possible complexity, with the name being indicative of the functionality that is implemented . The C source file can be named main.c for convenience. Currently, the makefile in each source directory is created to compile main.c and produce main.elf and main.dis (an object dump of the elf). Consequently, if you choose to name your new source file main.c, the Makefile can be borrowed without any changes from another source directory under ref.