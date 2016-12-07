all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"继续完善 arm64 的 hmpcb 负载均衡调度器, 完善 STOP_MACHINE 机制..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
