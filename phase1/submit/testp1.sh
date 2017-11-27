#!/bin/bash
list=$(grep "dumpProcesses();" ./testcases/*)
pass=0
fail=0

function execute {
        if [[ $list =~ $1 ]]; then
                myfile=$1noDumps
        else
                myfile=$1
        fi  

        make ${myfile} > /dev/null
        ./${myfile} > ./testOutputs/${myfile}.txt

        if [[ $(diff ./testOutputs/${myfile}.txt ./testResults/${myfile}.txt | wc -l) -ne 0 ]]; then
                echo diff: $1 failed
		diff ./testOutputs/${myfile}.txt ./testResults/${myfile}.txt
		let fail+=1;
        else
                echo "diff: $1 test passed"
		let pass+=1
        fi
}

if [[ $1 != "test"?? && $# -ne 0 ]]; then
	echo "BAD ARG"
	exit
fi

if [[ $1 == "test"?? ]]; then
	execute $1
else
	for i in $(ls ./testResults/test??.txt); do
		base=$(basename $i)
		arg="${base%.*}"
		execute ${arg}
	done

	echo Passed ${pass} Failed ${fail}
fi
