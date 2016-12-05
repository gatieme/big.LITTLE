all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"改进了 HMP/HMPCB 负载均衡调度器的..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
