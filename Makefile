all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"添加/proc/task_detect查看percpu的task信息, 由选项HEVTASK_INTERFACE开启..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
