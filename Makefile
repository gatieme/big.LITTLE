all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"完善了统计信息sched_avg, 并修复了kernel/sched/fair.c中条件宏匹配的问题, 同时更改了部分文件的编码cp936-=>utf-8..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
