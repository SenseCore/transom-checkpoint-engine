/**
 * @file rdma_communicator.cpp
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-07-03
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "communicator/rdma_communicator.h"

#include "api/api.h"
#include "config/config.h"
#include "util/nic_helper.h"
#include "util/util.h"

using communicators::RdmaCommunicator;
using communicators::rdma_resources;
using communicators::cm_con_data_t;
using communicators::Endpoint;
using util::Util;
using buffer::Buffer;

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x) {
    return bswap_64(x);
}
static inline uint64_t ntohll(uint64_t x) {
    return bswap_64(x);
}
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll(uint64_t x) {
    return x;
}
static inline uint64_t ntohll(uint64_t x) {
    return x;
}
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

RdmaCommunicator::RdmaCommunicator(Endpoint ep, int fd) {
    dev_name_ = util::MultiNicHelper::Instance().ChooseNic();

    gid_idx_ = -1;
    ib_port_ = 1;

    port_ = ep.port();
    addr_ = ep.addr();

    fd_ = fd;

    memset(&res_, 0, sizeof(res_));
}

RdmaCommunicator::~RdmaCommunicator() {
    resources_destroy();
    shutdown(fd_, SHUT_RDWR);
    close(fd_);
    util::MultiNicHelper::Instance().ReleaseNic(dev_name_);
}

void RdmaCommunicator::Serve() {
    if (fd_ = socket(AF_INET, SOCK_STREAM, 0); fd_ < 0) {
        LOG_FATAL("RdmaCommunicator: Can't create socket: {}", strerror(errno));
    }

    struct sockaddr_in socket_addr;
    memset(&socket_addr, 0, sizeof(struct sockaddr_in));
    socket_addr.sin_family = AF_INET;
    socket_addr.sin_port = htons(port_);
    socket_addr.sin_addr.s_addr = INADDR_ANY;

    // reuse socket
    int on = 1;
    auto ret = setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (ret < 0) {
        LOG_FATAL("RdmaCommunicator: Can't set socket option: {}", strerror(errno));
    }

    ret = bind(fd_, (struct sockaddr *)&socket_addr, sizeof(struct sockaddr_in));
    if (ret != 0) {
        LOG_FATAL("RdmaCommunicator: Can't bind socket: {}, address: {}, port: {}", strerror(errno), INADDR_ANY, port_);
    }

    ret = listen(fd_, 10);
    if (ret < 0) {
        LOG_FATAL("RdmaCommunicator: Can't listen from socket: {}", strerror(errno));
    }
}

bool RdmaCommunicator::Connect() {
    struct sockaddr_in remote;
    if (fd_ = socket(AF_INET, SOCK_STREAM, 0); fd_ <= 0) {
        LOG_FATAL("RdmaCommunicator: Can't create socket: {}", strerror(errno));
    }

    // Convert IPv4 and IPv6 addresses from text to binary form
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port_);
    if (inet_pton(AF_INET, addr_.c_str(), &remote.sin_addr) <= 0) {
        LOG_FATAL("RdmaCommunicator: invalid server address {}", addr_);
    }

    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &remote.sin_addr, addr, INET_ADDRSTRLEN);

    if (connect(fd_, (struct sockaddr *)&remote, sizeof(struct sockaddr_in)) != 0) {
        LOG_ERROR("RdmaCommunicator: Can't connect to socket {}:{}, {} ", addr, port_, strerror(errno));
        return false;
    }
    LOG_INFO("RdmaCommunicator: connected to {}:{}", addr, port_);
    return true;
}

std::shared_ptr<RdmaCommunicator> RdmaCommunicator::Accept() {
    unsigned fd;
    struct sockaddr_in client_socket_addr;
    unsigned client_socket_addr_size = sizeof(struct sockaddr_in);
    if (fd = accept(fd_, reinterpret_cast<sockaddr *>(&client_socket_addr), &client_socket_addr_size);
        fd <= 0 || errno == EINTR) {
        LOG_ERROR("RdmaCommunicator: cannot accept request: {}", strerror(errno));
        return nullptr;
    }

    auto res = std::make_shared<RdmaCommunicator>(Endpoint(addr_, port_), fd);
    return res;
}

/**
 * @brief read data from socket, store data in buffer
 *
 * @param buffer
 * @return
 */
bool RdmaCommunicator::Read(Buffer &buffer) {
    if (auto length = buffer.GetBufferSize(); length != 0) {
        LOG_FATAL("FATAL: only allow reading data into an empty buffer, this buffer has data length {}", length);
    }

    size_t recv_ret = 0;

    /* recv msg size */
    size_t msg_size = 0;
    recv_ret = sock_recv(reinterpret_cast<char *>(&msg_size), sizeof(msg_size));
    if (recv_ret <= 0) {
        goto error_check;
    }
    // LOG_TRACE("RdmaCommunicator: recved msg size {}", msg_size);

    /* receive into buffer */
    buffer.Realloc(msg_size);
    recv_ret = sock_recv(buffer.GetBuffer(), msg_size);
    if (recv_ret <= 0) {
        goto error_check;
    }
    // LOG_TRACE("RdmaCommunicator: recved msg");
    buffer.SetBufferSize(msg_size);
    return true;

error_check:
    if (recv_ret < 0) {
        LOG_ERROR("RdmaCommunicator: recv msg error");
    } else {
        // LOG_WARN("RdmaCommunicator: maybe peer has disconnected, it's normal");
    }
    return false;
}

/**
 * @brief write data to socket, still need it so that connection can be closed, and ensure recv before send
 *
 * @param buffer data pointer
 * @param size data size
 * @return
 */
bool RdmaCommunicator::Write(buffer::Buffer &buffer) {
    size_t send_ret = 0;

    auto size = buffer.GetBufferSize();
    send_ret = sock_send((const char *)&size, sizeof(size));
    if (send_ret <= 0) {
        goto error_check;
    }
    // LOG_TRACE("RdmaCommunicator: sent msg size {}", size);

    send_ret = sock_send(buffer.GetBuffer(), size);
    if (send_ret <= 0) {
        goto error_check;
    }
    // LOG_TRACE("RdmaCommunicator: sent msg");
    return true;

error_check:
    if (send_ret < 0) {
        LOG_ERROR("RdmaCommunicator: recv msg error");
    } else {
        // LOG_WARN("RdmaCommunicator: maybe peer has disconnected, it's normal");
    }
    return false;
}

void RdmaCommunicator::Sync() {
    if (poll_completion() != 0) {
        LOG_FATAL("RdmaCommunicator: poll completion failed");
    }
}

void RdmaCommunicator::Close() {
    resources_destroy();
    shutdown(fd_, SHUT_RDWR);
    close(fd_);
}

int RdmaCommunicator::rdma_handshake(bool server, size_t local_addr, size_t size) {
    region_ = local_addr;
    size_ = size;

    auto rc = create_resource(server);
    if (!api::IsSuccess(rc)) {
        return rc;
    }
    rc = connect_qp(server);
    if (!api::IsSuccess(rc)) {
        return rc;
    }
    LOG_INFO("RdmaCommunicator: rdma handshake complete");
    return api::STATUS_SUCCESS;
}

int RdmaCommunicator::create_resource(bool server) {
    struct ibv_qp_init_attr qp_init_attr;
    struct ibv_device *ib_dev = nullptr;
    int i;
    int mr_flags = 0;
    int cq_size = config::RDMA_CQ_SIZE;
    int num_devices;
    std::string all_dev_name;

    LOG_INFO("searching for IB devices in host");
    /* get device names in the system */
    auto dev_list = ibv_get_device_list(&num_devices);
    if (!dev_list) {
        LOG_ERROR("failed to get IB devices list");
        goto resources_create_exit;
    }
    /* if there isn't any IB device in host */
    if (!num_devices) {
        LOG_ERROR("cannot find devices");
        goto resources_create_exit;
    }
    for (int i = 0; i < num_devices; i++) {
        all_dev_name += std::string(ibv_get_device_name(dev_list[i])) + " ";
    }
    LOG_DEBUG("found {} IB devices: {}", num_devices, all_dev_name.c_str());
    /* search for the specific device we want to work with */
    for (i = 0; i < num_devices; i++) {
        if (dev_name_.size() == 0) {
            dev_name_ = std::string(ibv_get_device_name(dev_list[i]));
            ib_dev = dev_list[i];
            LOG_WARN("IB device not specified, using first one found: {}", dev_name_);
            break;
        }
        if (strcmp(ibv_get_device_name(dev_list[i]), dev_name_.c_str()) == 0) {
            ib_dev = dev_list[i];
            break;
        }
    }
    /* if the device wasn't found in host */
    if (!ib_dev) {
        LOG_ERROR("IB device {} not found", dev_name_);
        goto resources_create_exit;
    }
    /* get device handle */
    res_.ib_ctx = ibv_open_device(ib_dev);
    if (!res_.ib_ctx) {
        LOG_ERROR("failed to open device {}", dev_name_);
        goto resources_create_exit;
    }
    /* We are now done with device list, free it */
    ibv_free_device_list(dev_list);
    dev_list = nullptr;
    ib_dev = nullptr;
    /* query port properties */
    if (ibv_query_port(res_.ib_ctx, ib_port_, &res_.port_attr)) {
        LOG_ERROR("ibv_query_port on port {} failed", ib_port_);
        goto resources_create_exit;
    }

    /* allocate Protection Domain */
    res_.pd = ibv_alloc_pd(res_.ib_ctx);
    if (!res_.pd) {
        LOG_ERROR("ibv_alloc_pd failed");
        goto resources_create_exit;
    }
    /* each side will send only one WR, so Completion Queue with 1 entry is enough */
    res_.cq = ibv_create_cq(res_.ib_ctx, cq_size, NULL, NULL, 0);
    if (!res_.cq) {
        LOG_ERROR("failed to create CQ with {} entries", cq_size);
        goto resources_create_exit;
    }
    /* register the allocated memory buffer */
    mr_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
    res_.mr = ibv_reg_mr(res_.pd, reinterpret_cast<void *>(region_), size_, mr_flags);
    if (!res_.mr) {
        LOG_DEBUG("_region: {} _size: {} dev_name: {} pd_handle: {}",
                  reinterpret_cast<void *>(region_), size_, res_.pd->context->device->dev_name, res_.pd->handle);
        LOG_ERROR("ibv_reg_mr failed with mr_flags={}", mr_flags);
        goto resources_create_exit;
    }

    /* create the Queue Pair */
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.sq_sig_all = 1;
    qp_init_attr.send_cq = res_.cq;
    qp_init_attr.recv_cq = res_.cq;
    qp_init_attr.cap.max_send_wr = config::RDMA_CQ_SIZE;
    qp_init_attr.cap.max_recv_wr = config::RDMA_CQ_SIZE;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    res_.qp = ibv_create_qp(res_.pd, &qp_init_attr);
    if (!res_.qp) {
        LOG_ERROR("failed to create QP");
        goto resources_create_exit;
    }
    return api::STATUS_SUCCESS;

resources_create_exit:
    /* Error encountered, cleanup */
    if (res_.qp) {
        ibv_destroy_qp(res_.qp);
        res_.qp = NULL;
    }
    if (res_.mr) {
        ibv_dereg_mr(res_.mr);
        res_.mr = NULL;
    }
    if (res_.cq) {
        ibv_destroy_cq(res_.cq);
        res_.cq = NULL;
    }
    if (res_.pd) {
        ibv_dealloc_pd(res_.pd);
        res_.pd = NULL;
    }
    if (res_.ib_ctx) {
        ibv_close_device(res_.ib_ctx);
        res_.ib_ctx = NULL;
    }
    if (dev_list) {
        ibv_free_device_list(dev_list);
        dev_list = NULL;
    }
    LOG_ERROR("handshake failed");
    return api::STATUS_UNKNOWN_ERROR;
}

int RdmaCommunicator::connect_qp(bool server) {
    struct cm_con_data_t local_con_data;
    struct cm_con_data_t remote_con_data;
    struct cm_con_data_t tmp_con_data;
    int rc = 0;
    char temp_char;
    union ibv_gid my_gid;
    if (gid_idx_ >= 0) {
        rc = ibv_query_gid(res_.ib_ctx, ib_port_, gid_idx_, &my_gid);
        if (rc) {
            LOG_ERROR("could not get gid for port {}, index {}", ib_port_, gid_idx_);
            return api::STATUS_UNKNOWN_ERROR;
        }
    } else {
        memset(&my_gid, 0, sizeof(my_gid));
    }
    /* exchange using TCP sockets info required to connect QPs */
    local_con_data.addr = htonll((uintptr_t)region_);
    local_con_data.rkey = htonl(res_.mr->rkey);
    local_con_data.qp_num = htonl(res_.qp->qp_num);
    local_con_data.lid = htons(res_.port_attr.lid);
    memcpy(local_con_data.gid, &my_gid, 16);
    if (sock_sync_data(sizeof(struct cm_con_data_t), reinterpret_cast<char *>(&local_con_data),
                       reinterpret_cast<char *>(&tmp_con_data))
        < 0) {
        LOG_ERROR("failed to exchange connection data between sides");
        return api::STATUS_UNKNOWN_ERROR;
    }
    remote_con_data.addr = ntohll(tmp_con_data.addr);
    remote_con_data.rkey = ntohl(tmp_con_data.rkey);
    remote_con_data.qp_num = ntohl(tmp_con_data.qp_num);
    remote_con_data.lid = ntohs(tmp_con_data.lid);
    memcpy(remote_con_data.gid, tmp_con_data.gid, 16);
    /* save the remote side attributes, we will need it for the post SR */
    res_.remote_props = remote_con_data;
    LOG_TRACE("Local address = {}", (void *)region_);
    LOG_TRACE("Remote address = {}", (void *)remote_con_data.addr);
    // LOG_TRACE("Remote rkey = {}", (uint32_t)remote_con_data.rkey);
    // LOG_TRACE("Remote QP number = {}", (unsigned int)remote_con_data.qp_num);
    // LOG_TRACE("Remote LID = {}", (uint16_t)remote_con_data.lid);
    if (gid_idx_ >= 0) {
        uint8_t *p = remote_con_data.gid;
        LOG_TRACE("Remote GID ={}:{}:{}:{}:{}:{}:{}:{}:{}:{}:{}:{}:{}:{}:{}:{}", p[0],
                  p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
    }

    /* modify the QP to init */
    rc = modify_qp_to_init();
    if (rc) {
        LOG_ERROR("change QP state to INIT failed");
        return api::STATUS_UNKNOWN_ERROR;
    }
    rc = modify_qp_to_rtr(remote_con_data.qp_num, remote_con_data.lid, remote_con_data.gid);
    if (rc) {
        LOG_ERROR("failed to modify QP state to RTR");
        return api::STATUS_UNKNOWN_ERROR;
    }
    rc = modify_qp_to_rts();
    if (rc) {
        LOG_ERROR("failed to modify QP state to RTR");
        return api::STATUS_UNKNOWN_ERROR;
    }
    // LOG_DEBUG("QP state was change to RTS");
    /* sync to make sure that both sides are in states that they can connect to prevent packet loose */
    /* just send a dummy char back and forth */
    if (sock_sync_data(1, "Q", &temp_char)) {
        LOG_ERROR("sync error after QPs are were moved to RTS");
        return api::STATUS_UNKNOWN_ERROR;
    }

    return api::STATUS_SUCCESS;
}

/**
 * @brief Transition a QP from the RESET to INIT state
 *
 * @param qp
 * @return
 */
int RdmaCommunicator::modify_qp_to_init() {
    struct ibv_qp_attr attr;
    int flags;
    int rc;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_INIT;
    attr.port_num = ib_port_;
    attr.pkey_index = 0;
    attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
    flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
    rc = ibv_modify_qp(res_.qp, &attr, flags);
    if (rc)
        LOG_ERROR("failed to modify QP state to INIT");
    return rc;
}

/**
 * @brief Transition a QP from the INIT to RTR state, using the specified QP number
 *
 * @param qp QP to transition
 * @param remote_qpn remote QP number
 * @param dlid destination LID
 * @param dgid destination GID (mandatory for RoCEE)
 * @return 0 on success, ibv_modify_qp failure code on failure
 */
int RdmaCommunicator::modify_qp_to_rtr(uint32_t remote_qpn, uint16_t dlid, uint8_t *dgid) {
    struct ibv_qp_attr attr;
    int flags;
    int rc;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = IBV_MTU_256;
    attr.dest_qp_num = remote_qpn;
    attr.rq_psn = 0;
    attr.max_dest_rd_atomic = 1;
    attr.min_rnr_timer = 0x12;
    attr.ah_attr.is_global = 0;
    attr.ah_attr.dlid = dlid;
    attr.ah_attr.sl = 0;
    attr.ah_attr.src_path_bits = 0;
    attr.ah_attr.port_num = ib_port_;
    if (gid_idx_ >= 0) {
        attr.ah_attr.is_global = 1;
        attr.ah_attr.port_num = 1;
        memcpy(&attr.ah_attr.grh.dgid, dgid, 16);
        attr.ah_attr.grh.flow_label = 0;
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.sgid_index = gid_idx_;
        attr.ah_attr.grh.traffic_class = 0;
    }
    flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN
            | IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
    rc = ibv_modify_qp(res_.qp, &attr, flags);
    if (rc)
        LOG_ERROR("failed to modify QP state to RTR\n");
    return rc;
}

/**
 * @brief Transition a QP from the RTR to RTS state
 *
 * @param qp QP to transition
 * @return 0 on success, ibv_modify_qp failure code on failure
 */
int RdmaCommunicator::modify_qp_to_rts() {
    struct ibv_qp_attr attr;
    int flags;
    int rc;
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RTS;
    attr.timeout = 0x12;
    attr.retry_cnt = 6;
    attr.rnr_retry = 0;
    attr.sq_psn = 0;
    attr.max_rd_atomic = 1;
    flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT
            | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
    rc = ibv_modify_qp(res_.qp, &attr, flags);
    if (rc)
        fprintf(stderr, "failed to modify QP state to RTS\n");
    return rc;
}

/**
 * @brief post_receive
 *
 * @param res pointer to resources structure
 * @return 0 on success, error code on failure
 */
int RdmaCommunicator::post_receive(size_t addr, size_t size) {
    struct ibv_recv_wr rr;
    struct ibv_sge sge;
    struct ibv_recv_wr *bad_wr;
    int rc;
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    sge.addr = addr == 0 ? (uintptr_t)res_.mr->addr : addr;
    sge.length = size == 0 ? size_ : size;
    sge.lkey = res_.mr->lkey;
    /* prepare the receive work request */
    memset(&rr, 0, sizeof(rr));
    rr.next = NULL;
    rr.wr_id = 0;
    rr.sg_list = &sge;
    rr.num_sge = 1;
    /* post the Receive Request to the RQ */
    rc = ibv_post_recv(res_.qp, &rr, &bad_wr);
    if (rc)
        LOG_ERROR("failed to post RR");
    else
        LOG_DEBUG("Receive Request was posted");
    return rc;
}

/**
 * @brief This function will create and post a send work request
 *
 * @param opcode IBV_WR_SEND, IBV_WR_RDMA_READ or IBV_WR_RDMA_WRITE
 * @return 0 on success, error code on failure
 */
int RdmaCommunicator::post_send(int opcode, size_t addr, size_t remote_addr, uint32_t length) {
    length = length == 0 ? size_ : length;
    addr = addr == 0 ? region_ : addr;
    remote_addr = remote_addr == 0 ? res_.remote_props.addr : remote_addr;

    struct ibv_send_wr sr;
    struct ibv_sge sge;
    struct ibv_send_wr *bad_wr = NULL;
    int rc;

    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    sge.addr = addr;
    sge.length = length;
    sge.lkey = res_.mr->lkey;
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    sr.wr_id = 0;
    sr.sg_list = &sge;
    sr.num_sge = 1;
    sr.opcode = (ibv_wr_opcode)opcode;
    sr.send_flags = IBV_SEND_SIGNALED;
    if (opcode != IBV_WR_SEND) {
        sr.wr.rdma.remote_addr = remote_addr;
        sr.wr.rdma.rkey = res_.remote_props.rkey;
    }
    /* there is a Receive Request in the responder side, so we won't get any into RNR flow */
    rc = ibv_post_send(res_.qp, &sr, &bad_wr);
    if (rc)
        LOG_ERROR("failed to post SR, ret {}", rc);
    // else {
    //     switch (opcode) {
    //     case IBV_WR_SEND:
    //         LOG_DEBUG("Send Request was posted");
    //         break;
    //     case IBV_WR_RDMA_READ:
    //         LOG_DEBUG("RDMA Read Request was posted");
    //         break;
    //     case IBV_WR_RDMA_WRITE:
    //         LOG_DEBUG("RDMA Write Request was posted");
    //         break;
    //     default:
    //         LOG_DEBUG("Unknown Request was posted");
    //         break;
    //     }
    // }
    return rc;
}

int RdmaCommunicator::sock_sync_data(size_t xfer_size, const char *local_data, char *remote_data) {
    int rc;
    int read_bytes = 0;
    int total_read_bytes = 0;
    rc = write(fd_, local_data, xfer_size);
    if (rc < xfer_size)
        LOG_ERROR("Failed writing data during sock_sync_data");
    else
        rc = 0;
    while (!rc && total_read_bytes < xfer_size) {
        read_bytes = read(fd_, remote_data, xfer_size);
        if (read_bytes > 0)
            total_read_bytes += read_bytes;
        else
            rc = read_bytes;
    }
    return rc;
}

/**
 * @brief Poll the completion queue for a single event.
 *
 * @param res
 * @return 0 on success, <0 for poll failure, >0 for wc status error, 99 for QP_STATE_ABNORMAL
 */
int RdmaCommunicator::poll_completion() {
    struct ibv_wc wc;
    struct ibv_qp_attr attr;
    struct ibv_qp_init_attr init_attr;

    // poll until actual error or completion, empty CQ is considered continue
    int poll_iter = 0;
    int wait_time = config::RDMA_SLEEP_MIN_MILLISECONDS;
    while (true) {
        auto poll_result = ibv_poll_cq(res_.cq, 1, &wc);

        if (poll_result < 0) {
            /* poll CQ failed */
            LOG_ERROR("poll CQ failed");
            return poll_result;
        }

        if (poll_result > 0) {
            /* CQE found */
            // LOG_DEBUG("completion was found in CQ with status {}", wc.status);
            /* check the completion status (here we don't care about the completion opcode */
            if (wc.status != IBV_WC_SUCCESS) {
                LOG_ERROR("got bad completion with status: {}, vendor syndrome: {}", wc.status, wc.vendor_err);
                return static_cast<int>(wc.status);
            }
            return 0;
        }

        /* CQ empty */
        auto qp_ret = ibv_query_qp(res_.qp, &attr, IBV_QP_STATE, &init_attr);
        if (qp_ret != 0) {
            LOG_ERROR("failed to query QP state, ret {}", qp_ret);
            return qp_ret;
        }
        auto state = attr.cur_qp_state;
        if (state == IBV_QPS_RESET || state == IBV_QPS_ERR || state == IBV_QPS_UNKNOWN) {
            LOG_ERROR("qp state {}", state);
            return config::RDMA_QP_STATE_ABNORMAL;
        }
        /* go to the next round, sleep to avoid CPU overload */
        // LOG_TRACE("qp normal, job unfinished, continue...");
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        wait_time *= 2;
        wait_time = wait_time > config::RDMA_SLEEP_MAX_MILLISECOND ? config::RDMA_SLEEP_MAX_MILLISECOND : wait_time;
    }
}

void RdmaCommunicator::resources_destroy() {
    if (res_.qp) {
        if (ibv_destroy_qp(res_.qp)) {
            LOG_ERROR("failed to destroy QP");
        }
        res_.qp = nullptr;
    }
    if (res_.mr) {
        if (ibv_dereg_mr(res_.mr)) {
            LOG_ERROR("failed to deregister MR");
        }
        res_.mr = nullptr;
    }
    if (res_.cq) {
        if (ibv_destroy_cq(res_.cq)) {
            LOG_ERROR("failed to destroy CQ");
        }
        res_.cq = nullptr;
    }
    if (res_.pd) {
        if (ibv_dealloc_pd(res_.pd)) {
            LOG_ERROR("failed to deallocate PD");
        }
        res_.pd = nullptr;
    }
    if (res_.ib_ctx) {
        if (ibv_close_device(res_.ib_ctx)) {
            LOG_ERROR("failed to close device context");
        }
        res_.ib_ctx = nullptr;
    }
}

bool RdmaCommunicator::memory_registered(const void *ptr, size_t size) {
    auto lower_addr = reinterpret_cast<size_t>(ptr);
    auto upper_addr = lower_addr + size;
    auto lower_bound = region_;
    auto upper_bound = lower_bound + size;
    if (lower_addr >= lower_bound && upper_addr <= upper_bound) {
        return true;
    }

    // specially note if lower addr in range, upper addr out of range, it's illegal
    if (lower_addr >= lower_bound && lower_addr < upper_bound && upper_addr > upper_bound) {
        LOG_FATAL("memory [{}:{}] crosses region [{}:{}], which is illegal",
                  lower_addr, upper_addr, lower_bound, upper_bound);
    }

    return false;
}

/**
 * @brief Assume remote mr and local mr is enough. We're managing memory by ourself.
 * If memory is already in mr, send it by chunk directly.
 * Otherwise memcpy a chunk to mr and send.
 * Assume always client writes to remote.
 *
 * @param buffer memory to send
 * @param local_addr if memory is out of mr, copy it to local_addr, default to mr start point
 * @param remote_addr remote mr addr start point
 * @param size msg size, in bytes
 * @return true write success
 * @return false
 */
bool RdmaCommunicator::rdma_write(const char *buffer, size_t local_addr_offset,
                                  size_t remote_addr_offset, size_t size) {
    auto start_time = std::chrono::high_resolution_clock::now();

    size_t local_addr = region_ + local_addr_offset;
    // default to local mr size
    size = size == 0 ? size_ : size;
    size_t remote_addr = res_.remote_props.addr + remote_addr_offset;

    bool registered = memory_registered(buffer, size);
    if (!registered) {
        LOG_WARN("memory {} not registered, you may suffer poor performance due to memcpy", (void *)buffer);
        memcpy(reinterpret_cast<void *>(local_addr), buffer, size);
    }

    // write data by chunk
    int completions = 0;
    size_t written = 0;
    while (written < size) {
        auto to_write = size - written > config::RDMA_CHUNK_SIZE ? config::RDMA_CHUNK_SIZE : size - written;
        if (post_send(IBV_WR_RDMA_WRITE, local_addr + written, remote_addr + written, to_write) != 0) {
            LOG_ERROR("post_send IBV_WR_RDMA_WRITE failed");
            return false;
        }
        // LOG_TRACE("{}'the iter write {} data", completions, to_write);
        completions++;
        written += to_write;
    }

    // poll completions
    for (auto i = 0; i < completions; i++) {
        if (poll_completion() != 0) {
            LOG_ERROR("{}'th poll completion failed", completions);
            return false;
        }
    }

    auto timeval = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_time);
    LOG_INFO("RDMA performance: write {} bytes use {} milliseconds", size, timeval.count());

    return true;
}

/**
 * @brief Assume remote mr and local mr is enough. We're managing memory by ourself.
 * Read memory chunk by chunk.
 * Assume always client read from remote.
 *
 * @param buffer where data stores, data will not be deepcopied
 * @param local_addr local mr address
 * @param remote_addr remote mr address
 * @param size size to read
 * @return
 */
bool RdmaCommunicator::rdma_read(char *&buffer, size_t local_addr_offset, size_t remote_addr_offset, size_t size) {
    if (size <= 0) {
        LOG_ERROR("during rdma_read, size must be positive");
        return false;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // local mr + offset
    size_t local_addr = region_ + local_addr_offset;
    // remote mr + offset
    size_t remote_addr = res_.remote_props.addr + remote_addr_offset;

    // write by chunk
    int completions = 0;
    size_t read = 0;
    while (read < size) {
        auto to_read = size - read > config::RDMA_CHUNK_SIZE ? config::RDMA_CHUNK_SIZE : size - read;
        if (post_send(IBV_WR_RDMA_READ, local_addr + read, remote_addr + read, to_read) != 0) {
            LOG_ERROR("post_send IBV_WR_RDMA_READ failed");
            return false;
        }
        completions++;
        read += to_read;
    }

    // poll completions
    for (auto i = 0; i < completions; i++) {
        if (poll_completion() != 0) {
            LOG_ERROR("{}'th poll completion failed", completions);
            return false;
        }
    }

    bool registered = memory_registered(buffer, size);
    if (!registered) {
        LOG_WARN("memory {} not registered, you may suffer poor performance due to memcpy", (void *)buffer);
        memcpy(buffer, reinterpret_cast<void *>(local_addr), size);
    }

    auto timeval = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_time);
    LOG_INFO("RDMA performance: read {} bytes use {} milliseconds", size, timeval.count());

    return true;
}

/**
 * @brief send/recv signal, data maybe readable, in which case you can set size to 0. If you're transmitting binary
 * data, set size explicitly.
 *
 * Note that we do not recommend rdma_recv, due to sequence not guaranteed
 *
 * @param buffer data to send
 * @param local_addr local mr addr
 * @param remote_addr remote mr addr
 * @param size buffer size, default to buffer readable size
 * @return send success
 */
bool RdmaCommunicator::rdma_send(const char *buffer, size_t local_addr_offset, size_t remote_addr_offset, size_t size) {
    // local mr + offset
    size_t local_addr = region_ + local_addr_offset;
    // default to buffer readable size + 1
    size = size == 0 ? strlen(buffer) + 1 : size;
    // remote mr + offset
    size_t remote_addr = res_.remote_props.addr + remote_addr_offset;

    bool registered = memory_registered(buffer, size);
    if (!registered) {
        memcpy(reinterpret_cast<void *>(local_addr), buffer, size);
    }

    if (post_send(IBV_WR_SEND, local_addr, remote_addr, size) != 0) {
        LOG_ERROR("post_send IBV_WR_SEND error");
        return false;
    }
    return true;
}

/**
 * @brief send/recv signal, data maybe readable, in which case you can set size to 0. If you're transmitting binary
 * data, set size explicitly.
 *
 * Note that we do not recommend rdma_recv, due to sequence not guaranteed
 *
 * @param buffer where data stores, data will not be deepcopied, be careful
 * @param local_addr local mr addr
 * @param remote_addr remote mr addr
 * @param size buffer size, default to buffer readable size
 * @return send success
 */
bool RdmaCommunicator::rdma_recv(char **buffer, size_t local_addr_offset, size_t remote_addr_offset, size_t size) {
    if (size == 0) {
        LOG_ERROR("when rdma_recv, size must be set");
        return false;
    }

    // local mr + offset
    size_t local_addr = region_ + local_addr_offset;
    // remote mr + offset
    size_t remote_addr = res_.remote_props.addr + remote_addr_offset;

    if (post_receive(local_addr, size) != 0) {
        LOG_ERROR("post_receive error");
        return false;
    }

    *buffer = reinterpret_cast<char *>(local_addr);
    return true;
}

/**
 * @brief iterativelly recv data until recved 'size' bytes
 *
 * @param buffer store data in buffer
 * @param size expected size
 * @return -1 means failure, 0 means EOF(normal, maybe client disconnected), >0 means success, return actual recved bytes
 */
size_t RdmaCommunicator::sock_recv(char *buffer, size_t size) {
    size_t total_recved = 0;

    while (total_recved < size) {
        auto recved = recv(fd_, reinterpret_cast<void *>(reinterpret_cast<size_t>(buffer) + total_recved),
                           size - total_recved, MSG_WAITALL);
        if (recved < 0) {
            LOG_ERROR("socket `recv` return {}: {}", recved, strerror(errno));
            return recved;
        }
        if (recved == 0) {
            // LOG_WARN("EOF");
            return 0;
        }
        total_recved += recved;
    }
    return total_recved;
}

/**
 * @brief iterativelly send data until sent 'size' bytes
 *
 * @param buffer data to send
 * @param size expected size
 * @return -1 means failure, 0 means EOF(normal, maybe client disconnected), >0 means success, return actual recved bytes
 */
size_t RdmaCommunicator::sock_send(const char *buffer, size_t size) {
    size_t total_sent = 0;
    size_t sent = 0;

    while (total_sent < size) {
        auto sent = send(fd_, (const void *)(reinterpret_cast<size_t>(buffer) + total_sent),
                         size - total_sent, MSG_WAITALL);
        if (sent < 0) {
            LOG_ERROR("socket `send` return {}: {}", sent, strerror(errno));
            return sent;
        }
        if (sent == 0) {
            LOG_WARN("EOF");
            return 0;
        }
        total_sent += sent;
    }
    return total_sent;
}
