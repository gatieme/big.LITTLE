all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"创建了backup目录, 备份了fair.c文件, 同时在backup目录下将hmp和原本的cfs分割开..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
