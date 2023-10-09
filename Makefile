VERSION=1.0

help: ## Display this help.
	@awk 'BEGIN {FS = ":.*##"; printf "\nUsage:\n  make \033[36m<target>\033[0m\n"} /^[a-zA-Z_0-9-]+:.*?##/ { printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2 } /^##@/ { printf "\n\033[1m%s\033[0m\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

doc: ## generate document by doxygen
	doxygen Doxyfile

dev-build: ## build image for compiling
	docker build -t registry.cn-hangzhou.aliyuncs.com/sensecore-transom/tce-build:${VERSION}-rockylinux . -f dockerfile/rockylinux/Dockerfile.build
	docker build -t registry.cn-hangzhou.aliyuncs.com/sensecore-transom/tce-build:${VERSION}-ubuntu . -f dockerfile/ubuntu/Dockerfile.build
	docker build -t registry.cn-hangzhou.aliyuncs.com/sensecore-transom/tce-build:${VERSION}-centos . -f dockerfile/centos/Dockerfile.build

build: dev-build ## build image
	docker build -t registry.cn-hangzhou.aliyuncs.com/sensecore-transom/checkpoint-engine:${VERSION}-rockylinux . -f dockerfile/rockylinux/Dockerfile
	docker build -t registry.cn-hangzhou.aliyuncs.com/sensecore-transom/checkpoint-engine:${VERSION}-ubuntu . -f dockerfile/ubuntu/Dockerfile
	docker build -t registry.cn-hangzhou.aliyuncs.com/sensecore-transom/checkpoint-engine:${VERSION}-centos . -f dockerfile/centos/Dockerfile

bin: ## build on bare-metal
	./hack/build.sh
