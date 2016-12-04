all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"改进了HMP负载均衡调度器, 完成了编译..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
