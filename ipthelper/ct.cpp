#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_conntrack.h>
#include <linux/netfilter/nf_conntrack_tcp.h>
#include <linux/netfilter/nfnetlink_conntrack.h>
#include <string.h>
#include <net/if_arp.h>
#include <sys/wait.h>
#include <sched.h>
#include <sys/mount.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <libnl3/netlink/msg.h>
#include <assert.h>
#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"
#include "libnetlink.h"
#include "ct.h"

uint32_t filter_ipv4;

cr_filter* filter = NULL;
int filter_len;
int filter_af;

void conditional_free()
{
	if (filter == NULL) return;
	
	for (int i = 0; i < filter_len; i++)
	{
		if (filter[i].max != 0)
		{
			free(filter[i].internal);
		}
	}
	filter = NULL;
}
void conditional_init(int address_family, cr_filter* filters, int filters_len)
{
	if (filter != NULL)
	{
		conditional_free();
	}
	filter = filters;
	for (int i = 0; i < filters_len; i++)
	{
		if (filters[i].max != 0)
		{
			filters[i].internal = malloc(sizeof(nlattr *) * (filters[i].max + 1));
		}
	}
	filter_len = filters_len;
	filter_af = address_family;
}
bool conditional_filter(struct nlmsghdr *nlh)
{
	if (filter == NULL)
	{
		return true;
	}
	struct nlattr *tb[CTA_MAX + 1];
	struct nlattr ** tb_cur;
	struct nfgenmsg *msg;
	int err;
	char* data;
	bool ret = true;
	
	msg = (nfgenmsg *)NLMSG_DATA(nlh);

	if (msg->nfgen_family != filter_af)
		goto out;
	
	err = nlmsg_parse(nlh, sizeof(struct nfgenmsg), tb, CTA_MAX, NULL);
	if (err < 0)
		goto out;
	
	tb_cur = tb;
	for (int i = 0; i < filter_len; i++)
	{
		cr_filter* f = &filter[i];
		
		if (f->max != 0)
		{
			err = nla_parse_nested((struct nlattr **)f->internal, f->max, tb_cur[f->key], NULL);
			if (err < 0)
				goto out;
		
			tb_cur = (nlattr **)f->internal;
		}
		else if (f->compare_len != 0)
		{
			data = (char *)nla_data(tb_cur[f->key]);
			//printf("%s\n", inet_ntoa(*(in_addr*)data));
			
			ret = memcmp(data, f->compare, f->compare_len) == 0;
		}
	}
	
out:
	return ret;
}
static int dump_one_nf(struct nlmsghdr *hdr, void *arg)
{
	struct cr_img *img = (cr_img *)arg;
	
	if (!conditional_filter(hdr))
	{
		return 0;
	}

	cr_node* node = (cr_node*)malloc(sizeof(cr_node*) + hdr->nlmsg_len);
	memcpy(node->data,hdr,hdr->nlmsg_len);
	
	node->next = img->start;
	img->start = node;
		
	return 0;
}

static int ct_restore_callback(struct nlmsghdr *nlh)
{
	struct nfgenmsg *msg;
	struct nlattr *tb[CTA_MAX + 1], *tbp[CTA_PROTOINFO_MAX + 1], *tb_tcp[CTA_PROTOINFO_TCP_MAX + 1];
	int err;

	msg = (nfgenmsg *)NLMSG_DATA(nlh);

	if (msg->nfgen_family != AF_INET && msg->nfgen_family != AF_INET6)
		return 0;

	err = nlmsg_parse(nlh, sizeof(struct nfgenmsg), tb, CTA_MAX, NULL);
	if (err < 0)
		return -1;

	if (!tb[CTA_PROTOINFO])
		return 0;

	err = nla_parse_nested(tbp, CTA_PROTOINFO_MAX, tb[CTA_PROTOINFO], NULL);
	if (err < 0)
		return -1;

	if (!tbp[CTA_PROTOINFO_TCP])
		return 0;

	err = nla_parse_nested(tb_tcp, CTA_PROTOINFO_TCP_MAX, tbp[CTA_PROTOINFO_TCP], NULL);
	if (err < 0)
		return -1;

	if (tb_tcp[CTA_PROTOINFO_TCP_FLAGS_ORIGINAL]) {
		struct nf_ct_tcp_flags *flags;

		flags = (nf_ct_tcp_flags *)nla_data(tb_tcp[CTA_PROTOINFO_TCP_FLAGS_ORIGINAL]);
		flags->flags |= IP_CT_TCP_FLAG_BE_LIBERAL;
		flags->mask |= IP_CT_TCP_FLAG_BE_LIBERAL;
	}

	if (tb_tcp[CTA_PROTOINFO_TCP_FLAGS_REPLY]) {
		struct nf_ct_tcp_flags *flags;

		flags = (nf_ct_tcp_flags *)nla_data(tb_tcp[CTA_PROTOINFO_TCP_FLAGS_REPLY]);
		flags->flags |= IP_CT_TCP_FLAG_BE_LIBERAL;
		flags->mask |= IP_CT_TCP_FLAG_BE_LIBERAL;
	}

	return 0;
}

/*
Restore from buffer
*/
int restore_nf_cts(bool expectation, char* data, int data_len)
{
	struct nlmsghdr *nlh = NULL;
	int exit_code = -1, sk;

	sk = socket(AF_NETLINK, SOCK_RAW, NETLINK_NETFILTER);
	if (sk < 0) {
		pr_perror("Can't open rtnl sock for net dump");
		goto out_img;
	}

	for(int i=0;i<data_len;) {
		int ret;

		nlh = (struct nlmsghdr *)&data[i];
		
		if (!expectation)
			if (ct_restore_callback(nlh))
				goto out;

		nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE;
		ret = do_rtnl_req(sk, nlh, nlh->nlmsg_len, NULL, NULL, NULL);
		if (ret)
			goto out;
		
		i += nlh->nlmsg_len;
		assert(i <= data_len);
	}

	exit_code = 0;
out:
	close(sk);
out_img:
	return exit_code;
}

/**
Dump connections
*/
int dump_nf_cts(bool expectations, struct cr_img* out)
{
	struct cr_img img = {};
	struct {
		struct nlmsghdr nlh;
		struct nfgenmsg g;
	} req;
	int sk, ret;

	pr_info("Dumping netns links\n");

	ret = sk = socket(AF_NETLINK, SOCK_RAW, NETLINK_NETFILTER);
	if (sk < 0) {
		pr_perror("Can't open rtnl sock for net dump");
		*out = { };
		goto out;
	}

	memset(&req, 0, sizeof(req));
	req.nlh.nlmsg_len = sizeof(req);
	req.nlh.nlmsg_type = (NFNL_SUBSYS_CTNETLINK << 8);

	if (!expectations)
		req.nlh.nlmsg_type |= IPCTNL_MSG_CT_GET;
	else
		req.nlh.nlmsg_type |= IPCTNL_MSG_EXP_GET;

	req.nlh.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
	req.nlh.nlmsg_pid = 0;
	req.nlh.nlmsg_seq = CR_NLMSG_SEQ;
	req.g.nfgen_family = AF_UNSPEC;

	ret = do_rtnl_req(sk, &req, sizeof(req), dump_one_nf, NULL, &img);
	close(sk);
	
	*out = img;
out:
	return ret;

}

/**
Free all memory in the crimg linked list
*/
void cr_free(cr_img* img)
{
	struct cr_node* node = img->start;
	while (node != NULL)
	{
		struct cr_node* temp = node->next;
		free(node);
		node = temp;
	}
}

int cr_length(cr_node* node)
{
	struct nlmsghdr *hdr = (struct nlmsghdr *)node->data;	
	return hdr->nlmsg_len + sizeof(cr_node*);
}