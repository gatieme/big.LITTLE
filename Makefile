all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"初步完成big.LITTLE-mp下CONFIG_SCHED_HMP_ENHANCEMENT负载均衡调度器..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
