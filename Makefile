all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"修改体系结构arch/arm64/kernel/topology.c对CONFIG_SCHED_HMP_ENHANCEMENT改进的HMP负载调度器的设置..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
