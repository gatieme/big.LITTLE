all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"完善big.LITTLE-mp..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
