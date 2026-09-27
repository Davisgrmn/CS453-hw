# Homework 2

* Author: Davis Garman
* Class: CS453
* Semester: Fa26

## Overview

This assignment features an implementation of a memory allocator using a buddy system. It divides memory into partitions to store data as efficiently as possible by splitting it into halves.

## Compiling and Using

In order to compile and run tests, open a git bash terminal and use the following command in the hw2 directory:
$ make test

To delete dependency files:
$ make clean

To run valgrind:
$ make valgrind

## Results

All implemented tests in the testing suite pass.

(base) davis@DGlenovo:~/CS453-hw/hw2$ make valgrind
valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 build/allocator_test
==11135== Memcheck, a memory error detector
==11135== Copyright (C) 2002-2022, and GNU GPL'd, by Julian Seward et al.
==11135== Using Valgrind-3.22.0 and LibVEX; rerun with -h for copyright info
==11135== Command: build/allocator_test
==11135== 
Allocator tests passed (36,000 modeled operations, mapping failures included).
==11135== 
==11135== HEAP SUMMARY:
==11135==     in use at exit: 0 bytes in 0 blocks
==11135==   total heap usage: 1 allocs, 1 frees, 1,024 bytes allocated
==11135== 
==11135== All heap blocks were freed -- no leaks are possible
==11135== 
==11135== For lists of detected and suppressed errors, rerun with: -s
==11135== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 build/wrapper_test
==11150== Memcheck, a memory error detector
==11150== Copyright (C) 2002-2022, and GNU GPL'd, by Julian Seward et al.
==11150== Using Valgrind-3.22.0 and LibVEX; rerun with -h for copyright info
==11150== Command: build/wrapper_test
==11150== 
Wrapper tests passed.
==11150== 
==11150== HEAP SUMMARY:
==11150==     in use at exit: 0 bytes in 0 blocks
==11150==   total heap usage: 1 allocs, 1 frees, 1,024 bytes allocated
==11150== 
==11150== All heap blocks were freed -- no leaks are possible
==11150== 
==11150== For lists of detected and suppressed errors, rerun with: -s
==11150== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)