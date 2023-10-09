FROM registry.cn-hangzhou.aliyuncs.com/sensecore-transom/tce-build:1.0-centos

# build project
ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/lib64
ADD . /opt/checkpoint-engine
RUN cd /opt/checkpoint-engine \
    && rm -f transom_snapshot_server/include/communicator/http/http.pb.* \
    && rm -rf build \
    && ./hack/build.sh

FROM registry.cn-hangzhou.aliyuncs.com/sensecore-transom/tce-build:1.0-centos
ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/lib64
COPY --from=builder /opt/checkpoint-engine/build/transom_snapshot_server /usr/bin
ENTRYPOINT ["transom_snapshot_server"]

# docker build -t registry.cn-hangzhou.aliyuncs.com/sensecore-transom/checkpoint-engine:1.0-centos . -f dockerfile/centos/Dockerfile