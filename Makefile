all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"删除了一些垃圾文件..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
