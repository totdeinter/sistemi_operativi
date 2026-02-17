#!/bin/bash
# Use of validator:
# ./test_shell.sh your_program
# if the your_program parameter is missing it defaults to ./esercizio_c

if [ $# -lt 1 ]; then
	CPROG=./esercizio_c
else
	CPROG=$1
	if [[ "$CPROG" != ./* ]]; then
		CPROG="./$CPROG"
	fi
fi
if [ ! -x $CPROG ]; then
	echo "File $CPROG not existing or not executable"
	echo "Global result: fail"
	exit
fi

run_fail(){
    RUNID=$1
	FAIL_INIT=$2
	CHARS=$3
	shift; shift; shift
	INFILES=$*
	${CPROG} $CHARS $INFILES > $$ 2>/dev/null
	if [ $? -eq 0 ]; then
		RES1="-"
		GLOBAL_RES="fail"
	else 
		RES1="+"
	fi
	cut -d ' ' -f 3 $$ | grep 'Error' > /dev/null
	if [ $? -eq 0 -o "$FAIL_INIT" == "true" ]; then
		RES2="+"
	else 
		RES2="-"
		GLOBAL_RES="fail"
	fi	
	echo "${RES1}${RES2} Expecting error for $INFILES"
	if [ "$RES1" == "-" ]; then
		echo "*** Expecting failure run code"
	fi
	if [ "$RES2" == "-" ]; then
		echo "*** Expecting string 'Error' in oputput "
		echo "*** Got:"
		cut -d ' ' -f 3- $$ 
	fi
	rm -f $$
}

run(){
    RUNID=$1
	CHARS=$2
	shift
	shift
	INFILES=$*
	FILENAME=out_${RUNID}.txt
	#${CPROG} $CHARS $INFILES | cut -d' ' -f1,3- | tee $FILENAME
	#return
	${CPROG} $CHARS $INFILES > $$
	# Check 2nd column is numeric
	cut -d ' ' -f 2 $$ | grep -E '[^0-9]' > /dev/null
	if [ $? -eq 1 ]; then
		RES1="+"
	else
		RES1="-"
		GLOBAL_RES="fail"
	fi
	# Remove 2nd column
	cut -d ' ' -f 1,3- $$ > $$_clean
	diff $$_clean $FILENAME > /dev/null
	if [ $? -eq 0 ]; then
		RES2="+"
	else
		RES2="-"
		GLOBAL_RES="fail"
	fi
	echo "${RES1}${RES2} chars=$CHARS files=$INFILES"
	if [ "$RES1" == "-" ]; then
		echo "*** Expecting numeric column for PID, got:"
		cut -d ' ' -f 2 $$
	fi
	if [ "$RES2" == "-" ]; then
		echo "*** Expecting:"
		cat $FILENAME
		echo "*** Got:"
		cat $$_clean
	fi
	rm -f $$ $$_clean
}

GLOBAL_RES="success"
run r1 abc f.txt g.txt
run r2 abcde g.txt
run r3 abc f.txt g.txt
run r4 xk f.txt g.txt
run r5 . f.txt g.txt
run_fail r6 false abcde not_found
run_fail r7 false abcde f.txt not_found
run_fail r8 true abcde
#run r2 $INFILE abcdef
#run $INFILE i.
#run_fail /etc/shadow-

echo "Global result: $GLOBAL_RES"
