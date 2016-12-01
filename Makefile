all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"添加小任务封包机制CONFIG_SCHED_HMP_LITTLE_PACKING..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
