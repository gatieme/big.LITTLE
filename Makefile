all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"修复了单个HMP模式下if-endif宏匹配的问题..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
