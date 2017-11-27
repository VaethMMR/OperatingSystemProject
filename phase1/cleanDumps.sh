#!/bin/bash
files=$(grep -l "dumpProcesses();" ./testcases/*)
for i in ${files} ; do
	myfile=${i%.*}
	grep -v "dumpProcesses();" $i > ${myfile}noDumps.c
done
