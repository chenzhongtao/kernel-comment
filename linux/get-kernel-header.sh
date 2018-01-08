#!/bin/bash

KERNEL_PATH=`pwd`/
# NR==5,NR==$NF {print $0}  打印第五到最后一行 也可以 NR>=5 {print $0}   sed -e 's/:/\n/g' 冒号改为换行  sed -e 's/\ .*\ //g 空格前的字符
# "    $(wildcard include/config/debug/highmem.h) \"  grep 之后  "    $(wildcard include/config/debug/highmem.h"  sed 之后 "include/config/debug/highmem.h"
# "    include/linux/poll.h \"  grep 之后  "    include/linux/poll.h"  sed 之后 "include/linux/poll.h"
 find . -name "*\.cmd"|xargs -i awk 'NR==5,NR==$NF {print $0}' {}|grep -o "\ .*\ .*\.h"|sed -e 's/\ .*\ //g'|sort|uniq >temp.txt

#  ./include/linux/autoconf.h
echo `find ./include/ -name "autoconf\.h"` >>temp.txt

# sed -e "s,${KERNEL_PATH},,g"  去掉绝对路径    
sed -e "s,${KERNEL_PATH},,g" temp.txt|grep "^\/" >other-file-header.txt

mkdir -p other-file-header
rm -f other-file-header/*
while read -r line
do
	cp "$line" other-file-header/
done < other-file-header.txt

sed -e "s,${KERNEL_PATH},,g" temp.txt|grep -v "^\/" >kernel-header.txt

rm -f temp.txt other-file-header.txt

#transfer soft-link into real file
for i in `cat kernel-header.txt`
do
	echo `readlink -e $i` >>kernel-header1.txt
done

sed -e "s,${KERNEL_PATH},,g" kernel-header1.txt|grep -v "^$"|sed -e 's/\\/\//g'|sort|uniq >kernel-header.txt

rm -f kernel-header1.txt
