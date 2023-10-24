#!/bin/bash

PARENT_DIR="$(dirname "$(realpath "$0")")"
source $PARENT_DIR/set-env.sh

CHFSDIR1=/tmp/mnt
SCRIPTS_PATH=$ROOT_PATH/scripts/lab2

passed_cnt=0

test_chsf_mounted(){
	mount | grep -q "$CHFSDIR1"
	if [ $? -ne 0 ];
	then
		echo "FATAL: chfs not mounted"
		exit
	fi;
}
test_chsf_mounted

##################################################

test_part2_a(){
	$SCRIPTS_PATH/test-lab2-part2-a.pl $CHFSDIR1 | grep -q "Passed part2 A"
	if [ $? -ne 0 ];
	then
	    echo "Failed part2 A"
	else
	    passed_cnt=$((passed_cnt+1))
		echo "Passed part2 A"
	fi
	test_chsf_mounted
}

##################################################

test_part2_b(){
	$SCRIPTS_PATH/test-lab2-part2-b.pl $CHFSDIR1 | grep -q "Passed part2 B"
	if [ $? -ne 0 ];
	then
	    echo "Failed part2 B"
	else
	    passed_cnt=$((passed_cnt+1))
		echo "Passed part2 B"
	fi
	test_chsf_mounted
}

##################################################

test_part2_c(){
	$SCRIPTS_PATH/test-lab2-part2-c.sh $CHFSDIR1 | grep -q "Passed part2 C"
	if [ $? -ne 0 ];
	then
	    echo "Failed part2 C"
	else
	    passed_cnt=$((passed_cnt+1))
		echo "Passed part2 C"
	fi
	test_chsf_mounted
}

##################################################

if [ $# -eq 1 ]; then
	if [ "$1" = "A" ]; then
		test_part2_a	
	elif [ "$1" = "B" ]; then
		test_part2_b
	elif [ "$1" = "C" ]; then
		test_part2_c
	else
		echo "Unknown Part "$1" "
	fi
else
	test_part2_a	
	test_part2_b
	test_part2_c
	echo ""
	echo "Passed "$passed_cnt"/3 tests"
fi