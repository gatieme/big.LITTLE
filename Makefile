all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"增加了 CONFIG_HMP_DELAY_UP_MIGRATION 宏, 因为大核可能大多数时间在睡眠, 延迟向上迁移的进行, 可以更充分的利用 CPU..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
