/*
 * Copyright 2016 Kristian Evensen <kristian.evensen@gmail.com>
 *
 * This file is part of MBB ping. MBB ping is free software: you can
 * redistribute it and/or modify it under the terms of the Lesser GNU General
 * Public License as published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * MBB ping is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * MBB ping. If not, see http://www.gnu.org/licenses/.
 */


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <linux/if.h>
#include <linux/filter.h>
#include <linux/icmp.h>
#include <linux/icmpv6.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <sys/epoll.h>

#define INITIAL_SLEEP_SEC   5
#define NORMAL_SLEEP_SEC    1
#define LAST_SLEEP_SEC      10
#define BUF_SIZE            1000
#define NORMAL_REQUEST_SIZE 64
#define MAX_SIZE_IP_HEADER  60

static int32_t create_bpf_filter6(int32_t bpf_fd,
                                  struct sockaddr_in6 *aRemoteAddr,
                                  uint16_t icmp_id)
{
    struct sock_filter icmp6_filter[] = {
        //Compare type
        BPF_STMT(BPF_LD | BPF_B | BPF_ABS, 0),
        BPF_JUMP(BPF_JMP | BPF_JEQ, 0x81, 1, 0),
        //Compare ID
        //BPF_STMT(BPF_LD | BPF_H | BPF_ABS, 4),
        //BPF_JUMP(BPF_JMP | BPF_JEQ, icmp_id, 1, 0),
        //Return values
        BPF_STMT(BPF_RET | BPF_K, 0),
        BPF_STMT(BPF_RET | BPF_K, 0xffff),
    };

     struct sock_fprog bpf_code = {
        .len = sizeof(icmp6_filter) / sizeof(icmp6_filter[0]),
        .filter = icmp6_filter,
    };

    if (setsockopt(bpf_fd, SOL_SOCKET, SO_ATTACH_FILTER, &bpf_code, sizeof(bpf_code)) < 0) {
        perror("setsockopt (SO_ATTACH_FILTER)");
        return -1;
    }

    return 0;

}

static int create_bpf_filter(int32_t bpf_fd, struct sockaddr_in *remote,
        uint16_t icmp_id)
{
    struct sock_fprog bpf_code;
    struct sock_filter icmp_filter[] = {
        //Load icmp header offset into index register. Note that this is an
        //inconsistency between v4 and v6
        BPF_STMT(BPF_LDX | BPF_B | BPF_MSH, 0),
        //IND is X + K (see /net/core/filter.c), so relative.
        //Store code + type in accumulator
        BPF_STMT(BPF_LD | BPF_H | BPF_IND, 0),
        //Check that this is Echo reply (both code and type is 0)
        BPF_JUMP(BPF_JMP | BPF_JEQ, 0x0000, 0, 2),
        //Load ID into accumulator and compare
        BPF_STMT(BPF_LD | BPF_H | BPF_IND, 4),
        BPF_JUMP(BPF_JMP | BPF_JEQ, icmp_id, 1, 0),
        //Return the value stored in K. It is the number of bytes that will be
        //passed on (0 indicates that packet should be ignored).
        BPF_STMT(BPF_RET | BPF_K, 0),
        BPF_STMT(BPF_RET | BPF_K, 0xffff),
    };

    memset(&bpf_code, 0, sizeof(bpf_code));
    bpf_code.len = sizeof(icmp_filter) / sizeof(icmp_filter[0]);
    bpf_code.filter = icmp_filter;

    if (setsockopt(bpf_fd, SOL_SOCKET, SO_ATTACH_FILTER, &bpf_code, sizeof(bpf_code)) < 0) {
        perror("setsockopt (SO_ATTACH_FILTER)");
        return -1;
    }

    return 0;
}

static int resolve_addr(char *remote, struct sockaddr_storage *remote_addr, uint8_t family)
{
    struct addrinfo hints, *res = NULL;
    int retval = 0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;

    retval = getaddrinfo(remote, NULL, &hints, &res);
    if (retval) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retval));
        return -1;
    }

    //I only care about the first IP address
    if (res == NULL) {
        fprintf(stderr, "Could not resolve remote\n");
        return -1;
    }

    memcpy(remote_addr, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    return 0;
}

static uint16_t calc_csum(const uint8_t *buf, const uint16_t buflen)
{
    int32_t csum = 0;
    uint16_t i = 0;

    //Assumes that buflen is divisible by two
    for (i=0; i<buflen; i+= 2) {
        csum += *((uint16_t*) (buf + i));

        //Add the carry-on back at the right-most bit
        if (csum & 0x10000)
            csum = (csum & 0xFFFF) + (csum >> 16);
    }

    //Add last carry ons
    while (csum >> 16)
        csum = (csum & 0xFFFF) + (csum >> 16);

    //ones complement of the ones complement sum, reason for invert
    csum = ~csum;

    return csum;
}

static ssize_t send_ping(int32_t icmp_fd, uint8_t *buf, uint16_t seq,
        uint16_t sndlen)
{
    struct icmphdr *icmph = (struct icmphdr*) buf;
    //We do like nomal ping and keep timer in ICMP payload, gettimeofday is OK
    //for now
    struct timeval *t_pkt = (struct timeval*) (icmph + 1);

    icmph->type = ICMP_ECHO;
    icmph->code = 0;

    //Convention, use network byte order
    icmph->un.echo.id = htons(getpid() & 0xffff);
    icmph->un.echo.sequence = htons(seq);
    gettimeofday(t_pkt, NULL);
    //We recycle buffer, so remember to reset checksum
    icmph->checksum = 0;

    //We dont need to use htons here. The reason is that the sum will for
    //example be 0x1cf7 on a LE machine and 0xf71c on a BE machine. This will be
    //stored the same way in memory (LE will put f7 first)
    icmph->checksum = calc_csum(buf, sndlen);

    return send(icmp_fd, buf, sndlen, 0);
}

static ssize_t send_ping6(int32_t icmp_fd, uint8_t *buf, uint16_t seq,
        uint16_t sndlen)
{
    struct icmp6hdr *icmp6h = (struct icmp6hdr*) buf;
    //We do like nomal ping and keep timer in ICMP payload, gettimeofday is OK
    //for now
    struct timeval *t_pkt = (struct timeval*) (icmp6h + 1);

    icmp6h->icmp6_type = ICMPV6_ECHO_REQUEST;
    icmp6h->icmp6_code = 0;

    //Convention, use network byte order
    icmp6h->icmp6_dataun.u_echo.identifier = htons(getpid() & 0xffff);
    icmp6h->icmp6_dataun.u_echo.sequence = htons(seq);
    gettimeofday(t_pkt, NULL);

    //We recycle buffer, so remember to reset checksum
    icmp6h->icmp6_cksum = 0;

    //We dont need to use htons here. The reason is that the sum will for
    //example be 0x1cf7 on a LE machine and 0xf71c on a BE machine. This will be
    //stored the same way in memory (LE will put f7 first)
    //icmp6h->icmp6_cksum = calc_csum(buf, sndlen);

    return send(icmp_fd, buf, sndlen, 0);
}

//Will return > 0 on success, <= 0 on failure (recvmsg failed, incorrect
//checksum)
static ssize_t handle_ping_reply(int32_t icmp_fd, uint16_t bufsize,
        int16_t family)
{
    ssize_t numbytes = -1;
    //rcv_buf will contain IP header + ICMP message, so I need to add the
    //maximum size ip header to be sure that request will fit
    uint8_t rcv_buf[bufsize + MAX_SIZE_IP_HEADER];
    uint8_t cmsg_buf[sizeof(struct cmsghdr) + sizeof(struct timeval)];

    struct iovec iov;
    struct msghdr msgh;
    struct cmsghdr *cmsg = (struct cmsghdr*) cmsg_buf;

    struct iphdr *iph;
    struct icmphdr *icmph;
    struct icmp6hdr *icmph6;
    uint16_t rcvd_csum = 0;

    double rtt = 0.0;
    struct timeval t_now;
    //We might nead to read timeval from packet
    struct timeval *t_rcv_pkt, *t_snd_pkt;

    memset(rcv_buf, 0, sizeof(rcv_buf));
    iph = (struct iphdr*) rcv_buf;
    //Inconsistency between ICMP and ICMPv6 in kernel, v6 removes IP header
    icmph6 = (struct icmp6hdr*) rcv_buf;

    memset(&iov, 0, sizeof(iov));
    memset(&msgh, 0, sizeof(msgh));

    iov.iov_base = rcv_buf;
    iov.iov_len = sizeof(rcv_buf);
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    msgh.msg_control = cmsg;
    msgh.msg_controllen = sizeof(cmsg_buf);

    numbytes = recvmsg(icmp_fd, &msgh, 0);

    if (numbytes <= 0)
        return numbytes;

    if (family == AF_INET) {
        icmph = (struct icmphdr*) (rcv_buf + (iph->ihl*4));
        rcvd_csum = calc_csum((uint8_t*) icmph,
                ntohs(iph->tot_len) - (iph->ihl * 4));
    }

    //For now, just return -1 when checksum is failed
    if (rcvd_csum) {
        fprintf(stderr, "Got ICMP reply with corrupt checksum\n");
        return -1;
    }

    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMP) {
        //Use cmesg data for timestamp if available
        t_rcv_pkt = (struct timeval*) CMSG_DATA(cmsg);
        t_now.tv_sec = t_rcv_pkt->tv_sec;
        t_now.tv_usec = t_rcv_pkt->tv_usec;
    } else {
        gettimeofday(&t_now, NULL);
    }

    //When packet was sent is stored in packet on send
    if (family == AF_INET)
        t_snd_pkt = (struct timeval*) (icmph + 1);
    else
        t_snd_pkt = (struct timeval*) (icmph6 + 1);

    rtt = ((t_now.tv_sec - t_snd_pkt->tv_sec) * 1000000.0) +
        ((t_now.tv_usec - t_snd_pkt->tv_usec));

    if (family == AF_INET)
        printf("Received seq %u Rtt %.2fms\n", ntohs(icmph->un.echo.sequence),
                rtt / 1000.0);
    else
        printf("Received seq %u Rtt %.2fms\n",
                ntohs(icmph6->icmp6_dataun.u_echo.sequence), rtt / 1000.0);

    return numbytes;
}

//Returns number of returned packets, -1 if something fails
static int run_ping_event_loop(int32_t icmp_fd, int16_t family, uint32_t count,
        uint8_t first_sleep, uint8_t normal_sleep, uint16_t bufsize,
        uint16_t normal_bufsize)
{
    //Variables related to event loop
    struct timeval t_now, t_send;
    int32_t sleep_time = 0;
    int32_t epoll_fd = 0, nfds = 0;
    struct epoll_event ev;
    
    //Variables related to packet transmission
    uint8_t snd_buf[bufsize];
    int32_t rcvd_pkts = 0;

    //Sequence number can potentially overflow (count is int or when we let the
    //application run indefinetly). However, this is not a problem, as we do not
    //track seq
    uint16_t seq = 0;
    ssize_t numbytes = 0;

    //Configure epoll
    //Set up epoll
    if ((epoll_fd = epoll_create(1)) == -1) {
        perror("epoll_create");
        return -1;
    }

    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = icmp_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, icmp_fd, &ev) == -1) {
        perror("epoll_ctl");
        return -1;
    }

    //Set IP header to point correctly
    memset(snd_buf, 0, sizeof(snd_buf));

    if (family == AF_INET)
        numbytes = send_ping(icmp_fd, snd_buf, seq, bufsize);
    else
        numbytes = send_ping6(icmp_fd, snd_buf, seq, bufsize);

    //Send first packet and start timer
    if (numbytes < 0) {
        perror("sendto");
        return -1;
    } else {
        fprintf(stdout, "Sent pkt with seq %u\n", seq++);
    }
    
    gettimeofday(&t_now, NULL);
    
    //This is where I need to add a check for count == 1
    t_send.tv_sec = t_now.tv_sec + first_sleep;
    t_send.tv_usec = t_now.tv_usec;

    while (1) {
        sleep_time = ((t_send.tv_sec - t_now.tv_sec) * 1000) + ((t_send.tv_usec - t_now.tv_usec) / 1000);
        //printf("Will sleep for %d ms\n", sleep_time);

        //This can happen if we are unluck, for example if a packet arrives at exactly
        //the time we were supposed to time out. Then t_now > t_send
        if (sleep_time < 0)
            sleep_time = 0;

        //Wait for reply or timeout
        nfds = epoll_wait(epoll_fd, &ev, 1, sleep_time);

        if (nfds == -1) {
            perror("epoll_wait");
            return -1;
        } else if (nfds == 0) {
            //Sequence number also works a counter for number of packets sent
            //TODO: Replace this as seq is 16 bit, and count 32
            if (count > 0 && seq == count) {
                printf("Received %d/%d packets\n", rcvd_pkts, count);
                break;
            }

            if (family == AF_INET)
                numbytes = send_ping(icmp_fd, snd_buf, seq, normal_bufsize);
            else
                numbytes = send_ping6(icmp_fd, snd_buf, seq, normal_bufsize);

            if (numbytes < 0) {
                perror("sendto");
                return -1;
            }
            
            fprintf(stdout, "Sent pkt with seq %u\n", seq++);

            gettimeofday(&t_now, NULL);

            if (seq == count) {
                printf("Sent specified number of packets, waiting for replies\n");
                t_send.tv_sec = t_now.tv_sec + LAST_SLEEP_SEC;
            } else {
                t_send.tv_sec = t_now.tv_sec + normal_sleep;
            }

            t_send.tv_usec = t_now.tv_usec;
        } else {
            if (handle_ping_reply(icmp_fd, bufsize, family) > 0)
                rcvd_pkts++;

            if (count > 0 && rcvd_pkts == 0) {
                printf("Received all replies (%d)\n", rcvd_pkts);
                break;
            } else {
                //For now, we ignore return value from handle ping reply and
                //just continue trying to receive
                //Timer is computed once per iteration, so this is sufficient
                //for adjusting timer after receive (relative to network presc.)
                gettimeofday(&t_now, NULL);
            }
        }
    }

    return rcvd_pkts;
}

static int32_t create_icmp_socket(char *remote, char *local, int16_t family,
        int32_t protocol, const char *iface)
{
    struct sockaddr_storage remote_addr, local_addr;
    int32_t raw_fd = -1, yes = 1;

    //Resolve the host
    memset(&remote_addr, 0, sizeof(remote_addr));
    
    if (resolve_addr(remote, &remote_addr, family)) {
        fprintf(stderr, "Could not resolve remote addr\n");
        exit(EXIT_FAILURE);
    }

    if (local) {
        memset(&local_addr, 0, sizeof(local_addr));
        if (resolve_addr(local, &local_addr, family)) {
            fprintf(stderr, "Could not resolve local addr\n");
            exit(EXIT_FAILURE);
        }
    }

    if ((raw_fd = socket(family, SOCK_RAW, protocol)) == -1) {
        perror("socket");
        return -1;
    }

    //argv contains null-terminated strings, so it is safe to pass them to
    //setsockopt
    if (setsockopt(raw_fd, SOL_SOCKET, SO_BINDTODEVICE, iface, strlen(iface)) < 0) {
        perror("setsockopt (SO_BINDTODEVICE)");
        close(raw_fd);
        return -1;
    }

    if (setsockopt(raw_fd, SOL_SOCKET, SO_TIMESTAMP, &yes, sizeof(yes)) < 0) {
        perror("setsockopt (SO_TIMESTAMP)");
        close(raw_fd);
        return -1;
    }

    if (local && bind(raw_fd, (const struct sockaddr*) &local_addr, sizeof(local_addr))) {
        perror("bind");
        close(raw_fd);
        return -1;
    }

    if (connect(raw_fd, (const struct sockaddr *) &remote_addr, sizeof(remote_addr))) {
        perror("socket");
        close(raw_fd);
        return -1;
    }

    //Use lowest two bytes of getpid as ID
    //The reason we dont have a htons/ntohs here, is that we simply provide the
    //value we want to match. That is the same, irrespective of byte order. Note
    //that a htons is still required when inserting the pid into the packet.
    //Othwerwise, a LE machine will store the value inverse of what we expect

    if (family == AF_INET) {
        if (create_bpf_filter(raw_fd, (struct sockaddr_in*) &remote_addr, 
                    getpid() & 0xFFFF) < 0)
            exit(EXIT_FAILURE);
    } else {
         if (create_bpf_filter6(raw_fd, (struct sockaddr_in6*) &remote_addr, 
                    getpid() & 0xFFFF) < 0)
            exit(EXIT_FAILURE);
    }

    return raw_fd;
}

static void usage()
{
    printf("Supported arguments\n");
    printf("\t-6 : IPv6 ping\n");
    printf("\t-i : interface to bind to (required)\n");
    printf("\t-d : destination IP (required)\n");
    printf("\t-l : local IP to use\n");
    printf("\t-c : number of packets to send (default: run indefinitely\n");
    printf("\t-f : how long to sleep after first packet is sent (default: 5sec)\n");
    printf("\t-t : how long to wait between packets (default: 1 sec)\n");
    printf("\t-s : inital packet size (default: 1000 byte)\n");
    printf("\t-p : normal packet size (default: 64 byte)\n");
    printf("\t-h : this menu\n");
}

int main(int argc, char *argv[])
{
    int32_t raw_fd = 0, opt = 0, count = 0, tmp_val = 0, protocol = IPPROTO_ICMP;
    int16_t family = AF_INET;
    uint16_t bufsize = BUF_SIZE, normal_bufsize = NORMAL_REQUEST_SIZE;
    uint8_t first_sleep = INITIAL_SLEEP_SEC, normal_sleep = NORMAL_SLEEP_SEC;
    char *iface = NULL, *host = NULL, *local = NULL;

    while ((opt = getopt(argc, argv, "i:c:d:f:t:s:p:l:h6")) != -1) {
        switch (opt) {
        case '6':
            family = AF_INET6;
            protocol = 0x3A;
            break;
        case 'i':
            iface = optarg;
            break;
        case 'c':
            count = atoi(optarg);
            break;
        case 'l':
            local = optarg;
            break;
        case 'd':
            host = optarg;
            break;
        case 'h':
            usage();
            exit(EXIT_SUCCESS);
            break;
        case 'f':
            tmp_val = atoi(optarg);

            if (tmp_val < 0 || tmp_val > UINT8_MAX) {
                fprintf(stderr, "-f is invalid (0<f<256)\n");
                exit(EXIT_FAILURE);
            }

            first_sleep = tmp_val;
            break;
        case 't':
            tmp_val = atoi(optarg);

            if (tmp_val < 0 || tmp_val > UINT8_MAX) {
                fprintf(stderr, "-t is invalid (0<t<256)\n");
                exit(EXIT_FAILURE);
            }

            normal_sleep = tmp_val;
            break;
        case 's':
            tmp_val = atoi(optarg);

            if (tmp_val < 8 || tmp_val > UINT16_MAX) {
                fprintf(stderr, "-s is invalid (8<s<65536\n");
                exit(EXIT_FAILURE);
            }

            bufsize = tmp_val;
            break;
        case 'p':
            tmp_val = atoi(optarg);

            if (tmp_val < 8 || tmp_val > UINT16_MAX) {
                fprintf(stderr, "-p is invalid (8<p<65536)\n");
                exit(EXIT_FAILURE);
            }

            normal_bufsize = tmp_val;
            break;
        case '?':
        default:
            fprintf(stderr, "Unknown argument\n");
            usage();
            exit(EXIT_FAILURE);
        }
    }

    if (iface == NULL || host == NULL) {
        fprintf(stderr, "Missing argument\n");
        usage();
        exit(EXIT_FAILURE);
    }

    //normal bufsize must always be smaller than bufsize
    if ((bufsize != BUF_SIZE || normal_bufsize != NORMAL_REQUEST_SIZE) &&
            normal_bufsize > bufsize) {
        fprintf(stderr, "Normal bufsize is larger than maximum bufsize\n");
        exit(EXIT_FAILURE);
    }

    //strlen excludes 0 byte, so strlen(iface) has to be less than IFNAMSIZ
    //Kernel checks length > IFNAMSIZ - 1
    if (strlen(iface) >= IFNAMSIZ) {
        fprintf(stderr, "Interface name is longer than limit\n");
        exit(EXIT_FAILURE);
    }

    printf("Interface: %s Num. request: %d Host: %s\n", iface, count, host);

    raw_fd = create_icmp_socket(host, local, family, protocol, iface);

    if (raw_fd == -1)
        exit(EXIT_FAILURE);

    //Add return code
    run_ping_event_loop(raw_fd, family, count,
            first_sleep, normal_sleep, bufsize, normal_bufsize);
    
    exit(EXIT_FAILURE);
}
