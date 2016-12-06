all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"继续完善 CONFIG_HMP_DELAY_UP_MIGRATION 宏, 为改进的 hmpcb 调度器增加了延迟向上迁移的的支持, 但是目前与 out_force_up 冲突..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
