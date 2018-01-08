#!/bin/bash

KERNEL_PATH=`pwd`/
# NR==3,NR==4 {print $0} 打印第三，第四行 ，gerp -o 只输出匹配的字符串，而不是整行 sed -e 's/.*\ //g' 去掉前面的空格
find . -name "*\.cmd"|xargs -i awk 'NR==3,NR==4 {print $0}' {}|grep -o "\ .*\.[csS]"|sed -e 's/.*\ //g'|sort|uniq >kernel-src.txt
# NR==5,NR==$NF {print $0}  打印第五到最后一行 也可以 NR>=5 {print $0}   sed -e 's/:/\n/g' 冒号改为换行 
find . -name "*\.cmd"|xargs -i awk 'NR==5,NR==$NF {print $0}' {}|grep -o ".*\.[csSi]"|sed -e 's/:/\n/g' >>kernel-src.txt

# transfer soft-link into real file
for i in `cat kernel-src.txt`
do
	echo `readlink -e $i` >>kernel-src1.txt
done

# sed -e "s,${KERNEL_PATH},,g"  去掉绝对路径      grep -v "^$" 去掉所有空行  s/\\/\//g   \ 改为  /
sed -e "s,${KERNEL_PATH},,g" kernel-src1.txt|grep -v "^$"|sed -e 's/\\/\//g'|sort|uniq >kernel-src.txt

rm -f kernel-src1.txt

