#!/bin/bash

for i in {1..50}
do
    echo "RUN $i"

    cd ~/rl-hetnet

    python3 inference_ppo.py

    cd ~/ns-allinone-3.38/ns-3.38

    ./ns3 run "scratch/hetnet-mappo"
done
