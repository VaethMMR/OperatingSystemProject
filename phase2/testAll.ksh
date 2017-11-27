#/bin/bash
pass=0
fail=0

function execute {
	myfile=$1
        make ${myfile} > /dev/null
        ./${myfile} > ./testOutputs/${myfile}.txt

	printf "diff %s: " $1
        if [[ $(diff ./testOutputs/${myfile}.txt ./testResults/${myfile}.txt | wc -l) -ne 0 ]]; then
                echo failed
                diff ./testOutputs/${myfile}.txt ./testResults/${myfile}.txt
                let fail+=1
        else
                echo passed
                let pass+=1
        fi
}

#if [[ $# == 0 || [[ $# == 1 && $1 != "test"?? ]] [[ $# == 2 && $1 != "test"?? && $2 != "test"?? ]] ]]; then
#        echo "BAD ARG"
#        exit
#fi

if [[ $# == 1 && $1 == "test"?? ]]; then
        execute $1

elif [[ $# == 2 && $1 == "test"?? && $2 == "test"?? ]]; then
	low=$(echo $1 | tail -c 3)
	high=$(echo $2 |tail -c 3)
	for i in $(eval echo {$low..$high}); do
		if [ $i -lt "10" ]; then
			execute "test0"$i
		else
			execute "test"$i
		fi
	done
	
	echo Passed ${pass} Failed ${fail}

else
	make clean > /dev/null
	make > /dev/null
	cp ./testcases/term?.in .
        
	for i in $(ls ./testResults/test??.txt); do
                base=$(basename $i)
                arg="${base%.*}"
                execute ${arg}
        done

        echo Passed ${pass} Failed ${fail}
fi
