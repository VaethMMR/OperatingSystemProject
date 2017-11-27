#!/bin/ksh
passed=0
failed=0

for i in {0..9}; do
        ./testphase2.ksh "0"$i > ./out
        grep -e 'passed' -e 'failed' ./out
        let passed+=$(grep -c 'passed' ./out)
        let failed+=$(grep -c 'failed' ./out)
        rm ./out
done

for i in {10..44}; do
        ./testphase2.ksh $i > ./out
        grep -e 'passed' -e 'failed' ./out
        let passed+=$(grep -c 'passed' ./out)
        let failed+=$(grep -c 'failed' ./out)
        rm ./out
done

make clean > /dev/null

echo Passed ${passed}
echo Failed ${failed}
