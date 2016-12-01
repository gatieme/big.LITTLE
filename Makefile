all	: github
.PHONY	: github


GITHUB_COMMIT	:=	"添加Kconfig/arm[64]:ARCH_SCALE_INVARIANT_CPU_CAPACITY, 以支持体系结构底层直接对 CPU Frequency进行修改..."

github	:
	git pull
	git add -A
	git commit -m $(GITHUB_COMMIT)
	git push origin master
