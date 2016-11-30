all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"添加了Makefile..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
