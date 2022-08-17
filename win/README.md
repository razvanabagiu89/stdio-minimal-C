# STDIO library

## About

https://ocw.cs.pub.ro/courses/so/teme/tema-2

Implement a minimal approach of the stdio library with a struct called SO_FILE
to replace the FILE struct from the C official library alongside read/write functions.
Generate a dynamic library that will be used further on.

## How to run the tests

- make & run local bash script on Linux
- make all on Windows

## Structure & Flow

For solving the tasks I used the following:

#### so_stdio.c
- contains all the functions for the library with both implementations for
Linux and Windows

#### Struct SO_FILE description:
- read_buf - buffer used for processing of reading from file
- write_buf - buffer used for processing of writing to file
- bytes_read - no. of bytes already read from file or ready to
be read (buffering behaviour)
- bytes_write - no. of bytes already written to file or ready to
be written (buffering behaviour)
- is_eof_or_error - flag to check if we reached EOF or an error
through the flow
- fd - file descriptor of both OS
- read_crs & write_crs - cursors used for iterating through the
buffers
- read_cursor_file & write_cursor_file - cursors used for iterating
through the files
- is_read - flag to check if file is opened in read only mode
- branch - check last operation if it is read or write


#### Below you will find descriptions of every function's flow:
- so_fopen:
    - will alocate the SO_FILE struct and check for errors. The mode parameter
    decides on multiple branches how will the file be opened accordingly. After
    the opening of the file, the file descriptor will be checked for errors and if 
    there is no error the function will alocate everything and return the
    file pointer

- so_fclose:
    - will close the file linked to it's fd but only after it
    checks if there are no bytes left in the buffer. If there are,
    it will write them then close the file and check if it has
    closed successfully.

- so_fgetc:
    - if it's the first call, read first 4096 bytes (usual size
    for buffer), then proceed to return one by one. After all
    the bytes are read, reload the buffer.

- so_fputc:
    - write to buffer one by one characters, and when the buffer
    is full write it to the file.

- so_fread:
    - uses so_fgetc to read from the file and store it in ptr

- so_fwrite:
    - uses so_fputc to write from ptr into the file

- so_fileno:
    - returns the file descriptor associated with the file

- so_fflush:
    - checks if there are bytes left in the write buffer and
    proceeds to put them into the file if it is so. After that,
    resets all cursors to 0.

- so_fseek:
    - moves the cursors according to parameters. Also checks if
    there are bytes left in the write buffer to flush them or
    in the read buffer to clear it in order for a new read to
    be made.
- so_ftell:
    - returns the cursor in the file, if opened in read only mode
    then we are using the is_read variable to decide on that.

- so_feof:
    - checks the flag is_eof_or_error and decides on that.

- so_ferror:
    - calls so_feof and checks above flag

All above functions are checking for errors and treating them accordingly
by returning an error code and freeing the memory.
    
- GNUmakefile - Linux Makefile

- Makefile - Windows Makefile

# Observations

Some lines of code may seem truncated and that is because the coding style checker had problems with their length
although they do not have over 80 characters. This is why I had to truncate them.
