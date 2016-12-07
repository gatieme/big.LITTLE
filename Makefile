all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"继续完善 CONFIG_HMP_DELAY_UP_MIGRATION 宏, 将 hmp_idle_pull 函数包含在宏内部..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
