all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"继续完善 CONFIG_HMP_DELAY_UP_MIGRATION 宏, 发现warning: ‘hmp_cpu_keepalive_trigger’ defined but not used [-Wunused-function]..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
