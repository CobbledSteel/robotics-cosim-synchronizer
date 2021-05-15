#!/bin/bash

cycles=()
seconds=()
reqs=()
avgreqs=()

simtype=$1
resultdir="results-${simtype}"

if [ "$simtype" = "MIDAS" ]; then
    ranges=(100000 50000 20000 10000 5000 2000 1000 500 100 10 1)
else 
    ranges=(100000000 50000000 20000000 10000000 5000000 2000000 1000000 500000 200000 100000 50000 20000 10000 5000 2000 1000)
fi

echo "running ${#ranges[@]} tests"

mkdir -p "${resultdir}/"

for (( i=0; i<${#ranges[@]}; i++ ))
do
    #echo "$i: ${ranges[i]}"
    filename="${resultdir}/${ranges[$i]}-synchronizer.log"
    cycles+=(", ${ranges[$i]}")
    seconds+=(", $(grep "secs" ${filename} | awk 'NF>1{print $NF}')")
    reqs+=(", $(grep "Total Firesim Sync Requests" ${filename} | awk 'NF>1{print $NF}')")
    avgreqs+=(", $(grep "Average Firesim Sync Requests" ${filename} | awk 'NF>1{print $NF}')")
done

echo "Cycles per Step ${cycles[@]}"
echo "Runtime (s) ${seconds[@]}"
echo "Total Requests ${reqs[@]}"
echo "Average Requests ${avgreqs[@]}"
