#!/bin/bash

simtype=$1
resultdir="results-${simtype}"

echo "building synchronizer"
if [ "$simtype" = "MIDAS" ]; then
    # firesim cycles per tick to sweep
    ranges=(100000 50000 20000 10000 5000 2000 1000 500 100 10 1)
    gcc -DMIDAS synchronizer.c -o synchronizer
else 
    # firesim cycles per tick to sweep
    ranges=(100000000 50000000 20000000 10000000 5000000 2000000 1000000 500000 200000 100000 50000 20000 10000 5000 2000 1000)
    gcc synchronizer.c -o synchronizer
fi

echo "running ${#ranges[@]} tests"

mkdir -p "${resultdir}/"

for interval in ${ranges[@]}
do
    filename="${resultdir}/${interval}-synchronizer.log"
    echo "running ${simtype} simulation using interval ${interval}"
    echo "    logging to ${filename} ..."
    echo "    launching synchronizer..."
    ./synchronizer 10101 10100 $interval > ${filename} &
    syncPID=$!
    cd ../gazebo-cosim/
    echo "    launching gazebo..."
    gzserver world_step.world > /dev/null & 
    gazeboPID=$!
    echo "    launching FireSim"
    if [ "$simtype" = "MIDAS" ]; then
        ssh -i ~/firesim.pem centos@52.90.99.213 "cd ~/firesim && source sourceme-f1-full.sh && cd ~/firesim/sim && ./runpk.sh" &
    else 
        ssh -i ~/firesim.pem centos@52.90.99.213 "cd ~/firesim && source sourceme-f1-full.sh && firesim infrasetup && firesim runworkload" &
        simPID=$!
    fi
    wait ${syncPID}
    echo "    test complete, killing gazebo"
    kill ${gazeboPID}
    echo "    gazebo terminated"
    if [ "$simtype" = "MIDAS" ]; then
        ssh -i ~/firesim.pem centos@52.90.99.213 "pkill VFireSim-debug" &
    else 
        wait ${simPID}
    fi
    echo "    simulation shutdown"
    cd ../test_server/
done
