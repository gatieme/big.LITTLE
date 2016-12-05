all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"完善改进的hmp-cb负载均衡调度器..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
