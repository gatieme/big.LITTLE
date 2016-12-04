all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"改进的HMP负载均衡调度器, 备份了kernel/sched/fair.c..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
