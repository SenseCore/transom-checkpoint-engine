/**
 * @file rdma_communicator.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief a light-weighted rdma server implementation, using Tcp for signalling. This implementation is far from mature.
 * @version 0.1
 * @date 2023-06-29
 *
 * @copyright Copyright (c) 2023
 *
 */

#pragma once

#include <arpa/inet.h>
#include <byteswap.h>
#include <endian.h>
#include <ext/stdio_filebuf.h>
#include <inttypes.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "infiniband/verbs.h"
#include "rdma/rdma_cma.h"

#include "buffer/buffer.h"
#include "communicator/endpoint.h"
#include "logger/logger.h"

namespace communicators {
using buffer::Buffer;

/**
 * @brief structure to exchange data which is needed to connect the QPs
 */
struct cm_con_data_t {
    /**
     * @brief Buffer address
     */
    uint64_t addr;

    /**
     * @brief Remote key
     */
    uint32_t rkey;

    /**
     * @brief QP(Queue Pair) number
     */
    uint32_t qp_num;

    /**
     * @brief LID of the IB port
     */
    uint16_t lid;

    /**
     * @brief gid
     */
    uint8_t gid[16];
} __attribute__((packed));

/**
 * @brief structure of system resources
 */
struct rdma_resources {
    /**
     * @brief Device attributes
     */
    struct ibv_device_attr device_attr;

    /**
     * @brief IB port attributes
     *
     */
    struct ibv_port_attr port_attr;

    /**
     * @brief values to connect to remote side
     */
    struct cm_con_data_t remote_props;

    /**
     * @brief values to connect to remote side for signal
     */
    struct cm_con_data_t remote_props_sig;

    /**
     * @brief device handle
     */
    struct ibv_context *ib_ctx;

    /**
     * @brief Protect Domain handle
     */
    struct ibv_pd *pd;

    /**
     * @brief Completion Queue handle
     */
    struct ibv_cq *cq;

    /**
     * @brief Queue Pair handle
     */
    struct ibv_qp *qp;

    /**
     * @brief Memory Region handle for buf
     */
    struct ibv_mr *mr;
};

/**
 * @brief RdmaCommunicator is an class that implements a simple stream oriented
 * mechanism for socket & rdma communicating with two endpoints.
 * @details Communicator use a client/server approach, for having a Communicator server
 * the application must call Serve() and the Accept() for accepting the
 * connection by clients and communicating to them.
 * The client has to use just the Connect() method.
 * For sending and receiving signalling through the communicator is possible the use
 * the input and output stream. Warning: _never_ try to communicate through the
 * streams of a server Communicator, for communicating with the client the
 * Communicator returned from the Accept() must be used.
 * For sending and receiving data, call rdma verb wrapper, whose functions are started with `rdma_` prefix.
 */
class RdmaCommunicator {
public:
    /**
     * @brief RdmaCommunicator constructor
     * @param ep endpoint, generated from endpoint factory
     * @param fd socket fd, for simplcity, can be replaced by CM(Connection Manager)
     */
    explicit RdmaCommunicator(Endpoint ep, int fd = -1);
    ~RdmaCommunicator();

    /**
     * @brief Sets the communicator as a server.
     */
    void Serve();

    /**
     * @brief Accepts a new connection. The call to the first Accept() must follow a call to Serve().
     *
     * @return RdmaCommunicator
     */
    std::shared_ptr<RdmaCommunicator> Accept();

    /**
     * @brief Sets the communicator as a client and connects it to the end point used to build this Communicator
     * @return bool true means connect succeeds
     */
    bool Connect();

    /**
     * @brief First read the msg size, then read the message into buffer
     *
     * @param buffer A data structure to store data
     * @return bool true means operation is successful
     */
    bool Read(buffer::Buffer &buffer);

    /**
     * @brief Write data in buffer to socket
     *
     * @param buffer A data structure to store data
     * @return bool write result, false means error
     */
    bool Write(buffer::Buffer &buffer);

    /**
     * @brief To sync with IO, must called after Write()
     */
    void Sync();

    /**
     * @brief Closes the connection with the end point.
     */
    void Close();

    /* rdma verb wrapper */

    /**
     * @brief wrapper for rdma write operation
     *
     * @param buffer data to write
     * @param local_addr_offset local offset, to write only part of the data
     * @param remote_addr_offset remote address offset, to write to specified address
     * @param size amount of data to write
     * @return true means success
     */
    bool rdma_write(const char *buffer, size_t local_addr_offset, size_t remote_addr_offset, size_t size);

    /**
     * @brief wrapper for rdma read operation
     *
     * @param buffer store remote data into this char array
     * @param local_addr_offset local offset, to store data to specified address
     * @param remote_addr_offset remote offset, to read part of the data
     * @param size amount of data to read
     * @return true means success
     */
    bool rdma_read(char *&buffer, size_t local_addr_offset, size_t remote_addr_offset, size_t size);

    /**
     * @brief wrapper for rdma send operation
     *
     * @param buffer data to write
     * @param local_addr_offset local offset, to write only part of the data
     * @param remote_addr_offset remote address offset, to write to specified address
     * @param size amount of data to write
     * @return true means success
     */
    bool rdma_send(const char *buffer, size_t local_addr_offset, size_t remote_addr_offset, size_t size);

    /**
     * @brief wrapper for rdma recv operation
     *
     * @param buffer store remote data into this char array
     * @param local_addr_offset local offset, to store data to specified address
     * @param remote_addr_offset remote offset, to read part of the data
     * @param size amount of data to read
     * @return true means success
     */
    bool rdma_recv(char **buffer, size_t local_addr_offset, size_t remote_addr_offset, size_t size);

    /**
     * @brief wrapper for complex rdma handshake operations, including prepare MR, PD, QP, exchange info, etc.
     * @details current limitations: rebuild everything in handshake, ugly implementation
     *
     * @param server true means caller is rdma server
     * @param local_addr local MR address
     * @param size local MR size
     * @return 0: success
     */
    int rdma_handshake(bool server, size_t local_addr, size_t size);

private:
    std::string addr_;     /* TCP connection address */
    uint16_t port_;        /* TCP connection port */
    std::string dev_name_; /* local IB device name */
    int ib_port_;          /* local IB port to work with */
    int gid_idx_;          /* gid index to use */
    size_t region_;        /* Local IB memory region address for data transfer */
    size_t size_;          /* data memory region size, local and remote must be the same */
    bool need_gc_ = false; /* mr need free after destruction */

    int fd_;
    rdma_resources res_;

    /* rmda util for handshake */
    int create_resource(bool server = false);
    int connect_qp(bool server = false);
    int modify_qp_to_init();
    int modify_qp_to_rtr(uint32_t remote_qpn, uint16_t dlid, uint8_t *dgid);
    int modify_qp_to_rts();
    int post_send(int opcode, size_t addr = 0, size_t remote_addr = 0, uint32_t length = 0);
    int post_receive(size_t addr = 0, size_t size = 0);
    int poll_completion();
    void resources_destroy();
    bool memory_registered(const void *ptr, size_t size);
    int sock_sync_data(size_t xfer_size, const char *local_data, char *remote_data);

    /* util for socket operation */
    size_t sock_recv(char *buffer, size_t size);
    size_t sock_send(const char *buffer, size_t);
};
} // namespace communicators