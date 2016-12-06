all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"继续完善 CONFIG_HMP_DELAY_UP_MIGRATION 宏, 下一步增加 hmpcb 负载均衡调度器的 hmp_idle_pull 函数..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
