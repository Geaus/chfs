#!/bin/bash

##########################################
#  this file contains:
#   BLOBFILE TEST: test for BLOB big file...
###########################################

DIR=$1
TEST_FILE1=foo.txt
TEST_FILE2=${DIR}/foo.txt
SRCFILE=tmprand

dd if=/dev/urandom of=${SRCFILE} bs=1K count=400 >/dev/null 2>&1

dd if=${SRCFILE} of=${TEST_FILE1} bs=1K seek=3 count=30 >/dev/null 2>&1
dd if=${SRCFILE} of=${TEST_FILE2} bs=1K seek=3 count=30 >/dev/null 2>&1
diff ${TEST_FILE1} ${TEST_FILE2} >/dev/null 2>&1
if [ $? -ne 0 ];
then
        echo "Failed part2 D"
        exit
fi
echo "Passed part2 D"

rm ${TEST_FILE2} ${TEST_FILE1} ${SRCFILE}
