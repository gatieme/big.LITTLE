all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"继续完善 CONFIG_HMP_DELAY_UP_MIGRATION 宏, 在 kernel/sched/core.c 中使用 wake_for_idle_pull 变量的地方用宏包裹..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
