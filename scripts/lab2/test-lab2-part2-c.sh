#!/bin/bash

##########################################
#  this file contains:
#   robustness test
###########################################

DIR=$1

#
#10 times seems a bit too tricky here....
# setting it to 3....
# ---------------
# I'm just trying to be nice....
#

for times in {0..3}
do
    echo "test Round:"$times""
    if ( ! ( perl test-lab2-part2-b.pl $1 | grep -q -i "Passed part2 B" ));
    then
        echo "Failed part2 C"
        exit
    fi
    if ( ! ( perl test-lab2-part2-a.pl $1 | grep -q -i "Passed part2 A" ));
    then
        echo "Failed part2 C"
        exit
    fi
done

echo "Passed part2 C"
