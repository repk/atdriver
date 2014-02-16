CCTARGET := arm-repkhardfp-linux-musleabi-
KARCH := arm

KMFLAGS := ARCH=${KARCH} CROSS_COMPILE=${CCTARGET}

CC := ${CCTARGET}gcc
LD := ${CCTARGET}gcc
MAKE := make

CFLAGS := -W -Wall
LDFLAGS :=
KFLAGS :=

DESTDIR := ./build
