all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"备份 fair.c  至完整的 hmpcb 调度算法 backup/fair_mpcb.c"

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
