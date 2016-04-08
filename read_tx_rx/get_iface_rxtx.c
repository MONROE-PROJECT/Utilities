/* Copyright (c) 2016, Kristian Evensen <kristrev@celerway.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <libmnl/libmnl.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <linux/if_link.h>

static int nlattr_link_cb(const struct nlattr *attr, void *data)
{
    const struct nlattr **tb = data;
    int type = mnl_attr_get_type(attr);

    if (mnl_attr_type_valid(attr, IFLA_MAX) < 0)
        return MNL_CB_OK;

    tb[type] = attr;

    return MNL_CB_OK;
}

static int rtnl_link_cb(const struct nlmsghdr *nlh, void *data)
{
    struct nlattr *tb[IFLA_MAX + 1] = {};
    struct ifinfomsg *ifm = mnl_nlmsg_get_payload(nlh);
    struct rtnl_link_stats *stats;
    struct rtnl_link_stats64 *stats64;
   
    mnl_attr_parse(nlh, sizeof(*ifm), nlattr_link_cb, tb); 

    if (tb[IFLA_STATS64]) {
        stats64 = mnl_attr_get_payload(tb[IFLA_STATS64]);
        fprintf(stdout, "%llu %llu %llu %llu\n", stats64->rx_packets,
                stats64->rx_bytes, stats64->tx_packets, stats64->tx_bytes);
    } else if (tb[IFLA_STATS]) {
        stats = mnl_attr_get_payload(tb[IFLA_STATS]);
        fprintf(stdout, "%u %u %u %u\n", stats->rx_packets, stats->rx_bytes,
                stats->tx_packets, stats->tx_bytes);
    } else {
        return MNL_CB_ERROR;
    }

    return MNL_CB_OK;
}

int main(int argc, char *argv[])
{
    uint8_t buf[MNL_SOCKET_BUFFER_SIZE];
    ssize_t numbytes = 0;
    struct nlmsghdr *nlh;
    struct ifinfomsg *ifm;
    struct mnl_socket *mnl_sock = NULL;

    if (argc < 2 || strlen(argv[1]) > IFNAMSIZ) {
        fprintf(stderr, "Missing/incorrect parameter\n");
        exit(EXIT_FAILURE);
    }

    if ((mnl_sock = mnl_socket_open(NETLINK_ROUTE)) == NULL) {
        fprintf(stderr, "Could not create MNL socket\n");
        exit(EXIT_FAILURE);
    }

    if (mnl_socket_bind(mnl_sock, 0, MNL_SOCKET_AUTOPID) < 0) {
        fprintf(stderr, "Could not bind socket\n");
        exit(EXIT_FAILURE);
    }

    //Create message
    nlh = mnl_nlmsg_put_header(buf);
    nlh->nlmsg_type = RTM_GETLINK;
    nlh->nlmsg_flags = NLM_F_REQUEST;

    ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(struct ifinfomsg)); 
    ifm->ifi_family = AF_UNSPEC;
    mnl_attr_put_str(nlh, IFLA_IFNAME, argv[1]);

    numbytes = mnl_socket_sendto(mnl_sock, buf, nlh->nlmsg_len);

    if (numbytes < 0) {
        fprintf(stderr, "Could not send netlink message\n");
        exit(EXIT_FAILURE);
    }

    numbytes = mnl_socket_recvfrom(mnl_sock, buf, MNL_SOCKET_BUFFER_SIZE);

    if (numbytes < 0) {
        fprintf(stderr, "Could not receive netlink message\n");
        exit(EXIT_FAILURE);
    }

    if (mnl_cb_run(buf, nlh->nlmsg_len, 0, mnl_socket_get_portid(mnl_sock),
            rtnl_link_cb, NULL) != MNL_CB_OK) {
        fprintf(stderr, "Netlink parsing failed\n");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
