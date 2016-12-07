all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"备份 config 和 config.old..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
