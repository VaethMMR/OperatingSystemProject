#!/bin/ksh

if make submit; then
	rm *.h *.c Makefile
	tar -xvf phase3.tgz > /dev/null
	./testHomer.ksh
else
	echo make submit failed
fi
