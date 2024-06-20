# Copyright (c) 2024, Meili Authors 

#!/bin/bash

# example:
# bash ./run.sh -live 2 30
# sudo kill -9 $(pidof meili)

BINARY="./build/meili"
EAL_SUFFIX="-n 1 -a 0000:03:00.0,class=net:regex:compress,rxq_cqe_comp_en=0 -a 0000:03:00.1,class=net --file-prefix dpdk0"
REGEX_RULE_SET="-r ./rulesets/teakettle.rof2.binary"
CMD_SUFFIX_LIVE="--input-mode dpdk_port --dpdk-primary-port 0000:03:00.0 --dpdk-second-port 0000:03:00.1 -d rxp"

# Check the argument value and run the corresponding command
case $1 in
   -live) 
        case $2 in 
            1) COREMASK="-l0";;
            2) COREMASK="-l0,1";;
            3) COREMASK="-l0,1,2";;
            4) COREMASK="-l0,1,2,3";;
            5) COREMASK="-l0,1,2,3,4";;
            6) COREMASK="-l0,1,2,3,4,5";;
            7) COREMASK="-l0,1,2,3,4,5,6";;
            8) COREMASK="-l0,1,2,3,4,5,6,7";;
            *) echo "Invalid # of cores.";;
        esac;
        CMD="$BINARY -D \"$COREMASK $EAL_SUFFIX \" $CMD_SUFFIX_LIVE $REGEX_RULE_SET -c $2 -s $3";;
   *) echo "Invalid argument.";;
esac

# excute the command
eval "$CMD"

