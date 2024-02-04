CC=gcc
ARGS=
INCLUDE=src/include/*
SRC=src
ODIR=build/
NAME=main

build:
	CC ${SRC}/*.c ${FLAGS} -I ${INCLUDE} -O ${ODIR}/${NAME}
