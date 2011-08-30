#include "commands.h"
#include "packet.h"
#include "net.h"
#include "video.h"
#include "rtl8139.h"
#include "syscalls.h"
#include "cdfs.h"
#include "dcload.h"
#include "go.h"
#include "disable.h"
#include "scif.h"
#include "maple.h"

unsigned int our_ip;
unsigned int tool_ip;
unsigned char tool_mac[6];
unsigned short tool_port;

#define NAME "dcload-ip 1.0.3"

typedef struct {
    unsigned int load_address;
    unsigned int load_size;
    unsigned char map[16384];
} bin_info_t;

bin_info_t bin_info;

unsigned char buffer[COMMAND_LEN + 1024]; /* buffer for response */
command_t * response = (command_t *)buffer;

void cmd_reboot(ether_header_t * ether, ip_header_t * ip, udp_header_t * udp, command_t * command)
{
    booted = 0;
    running = 0;

    disable_cache();
    go(0x8c004000);
}

void cmd_execute(ether_header_t * ether, ip_header_t * ip, udp_header_t * udp, command_t * command)
{
    if (!running) {
	tool_ip = ntohl(ip->src);
	tool_port = ntohs(udp->src);
	memcpy(tool_mac, ether->src, 6);
	our_ip = ntohl(ip->dest);

	make_ip(ntohl(ip->src), ntohl(ip->dest), UDP_H_LEN + COMMAND_LEN, 17, (ip_header_t *)(pkt_buf + ETHER_H_LEN));
	make_udp(ntohs(udp->src), ntohs(udp->dest),(unsigned char *) command, COMMAND_LEN, (ip_header_t *)(pkt_buf + ETHER_H_LEN), (udp_header_t *)(pkt_buf + ETHER_H_LEN + IP_H_LEN));
	bb_tx(pkt_buf, ETHER_H_LEN + IP_H_LEN + UDP_H_LEN + COMMAND_LEN);
	
	if (!booted)
	    disp_info();
	else
	    clear_lines(96, 24, 0);
	draw_string(0, 96, "executing...", 0xffff);
	
	if (ntohl(command->size)&1)
	    *(unsigned int *)0x8c004004 = 0xdeadbeef; /* enable console */
	else
	    *(unsigned int *)0x8c004004 = 0xfeedface; /* disable console */
	if (ntohl(command->size)>>1)
	    cdfs_redir_enable();
	
	bb_stop();
	
	running = 1;

	disable_cache();
	go(ntohl(command->address));
    }
}

void cmd_loadbin(ip_header_t * ip, udp_header_t * udp, command_t * command)
{
    bin_info.load_address = ntohl(command->address);
    bin_info.load_size = ntohl(command->size);
    memset(bin_info.map, 0, 16384);

    our_ip = ntohl(ip->dest);
    
    make_ip(ntohl(ip->src), ntohl(ip->dest), UDP_H_LEN + COMMAND_LEN, 17, (ip_header_t *)(pkt_buf + ETHER_H_LEN));
    make_udp(ntohs(udp->src), ntohs(udp->dest),(unsigned char *) command, COMMAND_LEN, (ip_header_t *)(pkt_buf + ETHER_H_LEN), (udp_header_t *)(pkt_buf + ETHER_H_LEN + IP_H_LEN));
    bb_tx(pkt_buf, ETHER_H_LEN + IP_H_LEN + UDP_H_LEN + COMMAND_LEN);

    if (!running) {
	if (!booted)
	    disp_info();
	else
	    clear_lines(96, 24, 0);
	draw_string(0, 96, "receiving data...", 0xffff);
    }
}
 
void cmd_partbin(ip_header_t * ip, udp_header_t * udp, command_t * command)
{ 
    int index = 0;

    memcpy((unsigned char *)ntohl(command->address), command->data, ntohl(command->size));
    
    index = (ntohl(command->address) - bin_info.load_address) >> 10;
    bin_info.map[index] = 1;
}

void cmd_donebin(ip_header_t * ip, udp_header_t * udp, command_t * command)
{
    int i;
    
    for(i = 0; i < (bin_info.load_size + 1023)/1024; i++)
	if (!bin_info.map[i])
	    break;
    if ( i == (bin_info.load_size + 1023)/1024 ) {
	command->address = htonl(0);
	command->size = htonl(0);
    }	else {
	command->address = htonl( bin_info.load_address + i * 1024);
	if ( i == ( bin_info.load_size + 1023)/1024 - 1)
	    command->size = htonl(bin_info.load_size % 1024);
	else
	    command->size = htonl(1024);
    }
    
    make_ip(ntohl(ip->src), ntohl(ip->dest), UDP_H_LEN + COMMAND_LEN, 17, (ip_header_t *)(pkt_buf + ETHER_H_LEN));
    make_udp(ntohs(udp->src), ntohs(udp->dest),(unsigned char *) command, COMMAND_LEN, (ip_header_t *)(pkt_buf + ETHER_H_LEN), (udp_header_t *)(pkt_buf + ETHER_H_LEN + IP_H_LEN));
    bb_tx(pkt_buf, ETHER_H_LEN + IP_H_LEN + UDP_H_LEN + COMMAND_LEN);

    if (!running) {
	if (!booted)
	    disp_info();
	else
	    clear_lines(96, 24, 0);
	draw_string(0, 96, "idle...", 0xffff);
    }	
}

void cmd_sendbinq(ip_header_t * ip, udp_header_t * udp, command_t * command)
{
    int numpackets, i;
    unsigned char *ptr;
    unsigned int bytes_left;
    unsigned int bytes_thistime;

    bytes_left = ntohl(command->size);
    numpackets = (ntohl(command->size)+1023) / 1024;
    ptr = (unsigned char *)ntohl(command->address);
    
    memcpy(response->id, CMD_SENDBIN, 4);
    for(i = 0; i < numpackets; i++) {
	if (bytes_left >= 1024)
	    bytes_thistime = 1024;
	else
	    bytes_thistime = bytes_left;
	bytes_left -= bytes_thistime;
		
	response->address = htonl((unsigned int)ptr);
	memcpy(response->data, ptr, bytes_thistime);
	response->size = htonl(bytes_thistime);
	make_ip(ntohl(ip->src), ntohl(ip->dest), UDP_H_LEN + COMMAND_LEN + bytes_thistime, 17, (ip_header_t *)(pkt_buf + ETHER_H_LEN));
	make_udp(ntohs(udp->src), ntohs(udp->dest),(unsigned char *) response, COMMAND_LEN + bytes_thistime, (ip_header_t *)(pkt_buf + ETHER_H_LEN), (udp_header_t *)(pkt_buf + ETHER_H_LEN + IP_H_LEN));
	bb_tx(pkt_buf, ETHER_H_LEN + IP_H_LEN + UDP_H_LEN + COMMAND_LEN + bytes_thistime);
	ptr += bytes_thistime;
    }
    
    memcpy(response->id, CMD_DONEBIN, 4);
    response->address = htonl(0);
    response->size = htonl(0);
    make_ip(ntohl(ip->src), ntohl(ip->dest), UDP_H_LEN + COMMAND_LEN, 17, (ip_header_t *)(pkt_buf + ETHER_H_LEN));
    make_udp(ntohs(udp->src), ntohs(udp->dest),(unsigned char *) response, COMMAND_LEN, (ip_header_t *)(pkt_buf + ETHER_H_LEN), (udp_header_t *)(pkt_buf + ETHER_H_LEN + IP_H_LEN));
    bb_tx(pkt_buf, ETHER_H_LEN + IP_H_LEN + UDP_H_LEN + COMMAND_LEN);
}

void cmd_sendbin(ip_header_t * ip, udp_header_t * udp, command_t * command)
{
    our_ip = ntohl(ip->dest);

    if (!running) {
	if (!booted)
	    disp_info();
	else
	    clear_lines(96, 24, 0);
	draw_string(0, 96, "sending data...", 0xffff);
    }
    
    cmd_sendbinq(ip, udp, command);

    if (!running) {
	clear_lines(96, 24, 0);
	draw_string(0, 96, "idle...", 0xffff);
    }
}

void cmd_version(ip_header_t * ip, udp_header_t * udp, command_t * command)
{
    int i;

    i = strlen("DCLOAD-IP 1.0.3") + 1;
    memcpy(response, command, COMMAND_LEN);
    memcpy(response->data, "DCLOAD-IP 1.0.3", i);
    make_ip(ntohl(ip->src), ntohl(ip->dest), UDP_H_LEN + COMMAND_LEN + i, 17, (ip_header_t *)(pkt_buf + ETHER_H_LEN));
    make_udp(ntohs(udp->src), ntohs(udp->dest),(unsigned char *) response, COMMAND_LEN + i, (ip_header_t *)(pkt_buf + ETHER_H_LEN), (udp_header_t *)(pkt_buf + ETHER_H_LEN + IP_H_LEN));
    bb_tx(pkt_buf, ETHER_H_LEN + IP_H_LEN + UDP_H_LEN + COMMAND_LEN + i);
}

void cmd_retval(ip_header_t * ip, udp_header_t * udp, command_t * command)
{
    if (running) {
	make_ip(ntohl(ip->src), ntohl(ip->dest), UDP_H_LEN + COMMAND_LEN, 17, (ip_header_t *)(pkt_buf + ETHER_H_LEN));
	make_udp(ntohs(udp->src), ntohs(udp->dest),(unsigned char *) command, COMMAND_LEN, (ip_header_t *)(pkt_buf + ETHER_H_LEN), (udp_header_t *)(pkt_buf + ETHER_H_LEN + IP_H_LEN));
	bb_tx(pkt_buf, ETHER_H_LEN + IP_H_LEN + UDP_H_LEN + COMMAND_LEN);

	bb_stop();

	syscall_retval = ntohl(command->address);
	escape_loop = 1;
    }
}

void cmd_maple(ip_header_t * ip, udp_header_t * udp, command_t * command) {
    /* Send Maple packet and wait for response */
    char *res; char *buf = (char *)(udp->data+4); int i;
    clear_lines(96, 24, 0); draw_string(0, 96, "sending maple cmd...", 0xffff);
    do res = maple_docmd(buf[0],buf[1],buf[2],buf[3],buf[4],buf+5); while (*res == MAPLE_RESPONSE_AGAIN);

    /* Send response back over socket */
    clear_lines(96, 24, 0); draw_string(0, 96, "sending maple response...", 0xffff);
    make_ip(ntohl(ip->src), ntohl(ip->dest), UDP_H_LEN + ((res[0] < 0) ? 4 : (res[3]+1)<<2), 17, (ip_header_t *)(pkt_buf + ETHER_H_LEN));
    make_udp(ntohs(udp->src), ntohs(udp->dest),(unsigned char *)res, ((res[0] < 0) ? 4 : (res[3]+1)<<2), (ip_header_t *)(pkt_buf + ETHER_H_LEN), (udp_header_t *)(pkt_buf + ETHER_H_LEN + IP_H_LEN));
    bb_tx(pkt_buf, ETHER_H_LEN + IP_H_LEN + UDP_H_LEN + ((res[0] < 0) ? 4 : (res[3]+1)<<2));
    clear_lines(96, 24, 0); draw_string(0, 96, "idle...", 0xffff);
}
