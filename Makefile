## Copyright 2014 HSA Foundation Inc.  All Rights Reserved.
##
## HSAF is granting you permission to use this software and documentation (if
## any) (collectively, the "Materials") pursuant to the terms and conditions
## of the Software License Agreement included with the Materials.  If you do
## not have a copy of the Software License Agreement, contact the  HSA Foundation for a copy.
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
## 1. Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
## 2. Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
## FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
## CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
## LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
## FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE SOFTWARE.

HSA_RUNTIME_PATH?=/home/strollinger/hsa/runtime

LFLAGS= -Wl,--unresolved-symbols=ignore-in-shared-libs -L$(HSA_RUNTIME_PATH)/lib -lhsa-runtime64 -lhsa-runtime-ext64

CC := clang

CFLAGS= -c -I$(HSA_RUNTIME_PATH)/include -std=c11 -g

HSASM := HSAILasm

SHARED_FILES := main.c header.c

all: mmul mmul2d

mmul: mmul.o mmul.brig
	$(CC) $(LFLAGS) mmul.o -o mmul

mmul2d: mmul2d.o mmul2d.brig
	$(CC) $(LFLAGS) mmul2d.o -o mmul2d

mmul%.o: mmul%.c $(SHARED_FILES) mmul_common.c
	$(CC) $(CFLAGS) -o $@ $<

%.brig: %.hsail
	$(HSASM) -assemble -o=$@ $<


clean:
	rm -rf *.o *.brig mmul mmul2d
