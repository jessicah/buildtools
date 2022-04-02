# Makefile for jam

CXX = c++
CFLAGS = -O
EXENAME = ./jam0
TARGET = -o $(EXENAME)
HOST_SYSTEM=$(shell uname)

# Special flavors - uncomment appropriate lines

# NCR seems to have a broken readdir() -- use gnu
#CC = gcc

# AIX needs -lbsd, and has no identifying cpp symbol
# Use _AIX41 if you're not on 3.2 anymore.
#LINKLIBS = -lbsd
#CFLAGS = -D_AIX

# NT (with Microsoft compiler)
# Use FATFS if building on a DOS FAT file system
#Lib = $(MSVCNT)/lib
#Include = $(MSVCNT)/include
#CC = cl /nologo
#CFLAGS = -I $(Include) -DNT 
#TARGET = /Fejam0
#LINKLIBS = $(Lib)/oldnames.lib $(Lib)/kernel32.lib $(Lib)/libc.lib
#EXENAME = .\jam0.exe

# NT (with Microsoft compiler)
# People with DevStudio settings already in shell environment.
#CC = cl /nologo
#CFLAGS = -DNT 
#TARGET = /Fejam0
#EXENAME = .\jam0.exe

# Interix - gcc
#CC = gcc

# Cygwin - gcc & cygwin
#CC = gcc
#CFLAGS = -D__cygwin__

# MingW32
#CC = gcc
#CFLAGS = -DMINGW

# MPEIX
#CC = gcc
#CFLAGS = -I/usr/include -D_POSIX_SOURCE

# QNX rtp (neutrino)
#CC = gcc

# BeOS (R5 only, actually, but who cares)
ifeq ($(HOST_SYSTEM), BeOS)
	ifeq ($(shell sh -c "strings /system/lib/libroot.so | grep uname"), )
		LINKLIBS = -lnet
	endif
endif

SOURCES = \
	builtins.cpp \
	command.cpp compile.cpp execunix.cpp execvms.cpp expand.cpp \
	filent.cpp fileos2.cpp fileunix.cpp filevms.cpp glob.cpp hash.cpp \
	headers.cpp jam.cpp jambase.cpp jamgram.cpp lists.cpp make.cpp make1.cpp \
	newstr.cpp option.cpp parse.cpp pathunix.cpp pathvms.cpp regexp.cpp \
	rules.cpp scan.cpp search.cpp timestamp.cpp variable.cpp ninja.cpp

all: $(EXENAME)
	LINKLIBS=$(LINKLIBS) $(EXENAME)

$(EXENAME):
	$(CXX) $(TARGET) $(CFLAGS) $(SOURCES) $(LINKLIBS)

clean:
	rm -rf bin.linuxx86
	rm jam0

test:
	bin.linuxx86/jam
