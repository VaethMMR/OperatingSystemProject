#!/bin/ksh
passed=0
failed=0
n=3

for i in {0..9}; do
        ./testphase$n.ksh "0"$i > ./out
        grep -e 'passed' -e 'failed' ./out
        let passed+=$(grep -c 'passed' ./out)
        let failed+=$(grep -c 'differ' ./out)
        rm ./out
done

for i in {10..25}; do
        ./testphase$n.ksh $i > ./out
        grep -e 'passed' -e 'differ' ./out
        let passed+=$(grep -c 'passed' ./out)
        let failed+=$(grep -c 'differ' ./out)
        rm ./out
done

make clean > /dev/null

echo Passed ${passed}
echo Failed ${failed}
