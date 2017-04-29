#!/bin/bash

shadertype=-c
output=error
while [[ $# -gt 1 ]]
do
key=$1
case $key in
	--version)
		echo "Compiler v0.0.0"
		exit 0
	;;
	-o)
		output=$2
		shift
	;;
	-c|--compute)
		shadertype=-c
	;;
	-v|--vertex)
		shadertype=-v
	;;
	-f|--fragment)
		shadertype=-f
	;;
	-fv|--combined)
		shadertype=-fv
	;;
	*)
		# unknown option
		break
	;;
esac
shift
done

adbfile=/data/local/tmp/$RANDOM.shader
adbfilebin=$adbfile.bin
if [[ -n $1 ]]; then
sourcefile=$1
fi

adb push $sourcefile $adbfile
adb shell /data/local/tmp/compiler $shadertype $adbfile $adbfilebin

temp_file=$(mktemp)

adb pull $adbfilebin $temp_file
adb shell rm $adbfile $adbfilebin
xxd -ps $temp_file $output

exit 0
