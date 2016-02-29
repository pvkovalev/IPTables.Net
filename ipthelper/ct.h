#pragma once

#include <stdint.h>

extern "C"
{
	struct cr_node
	{
		struct cr_node* next;
		char data[1];
	};
	struct cr_img
	{
		struct cr_node* start;
	};
	
	struct cr_filter
	{
		int key; /* e.g CTA_PROTOINFO */
		int max; /* e.g CTA_PROTOINFO_MAX, or 0 if doing comparison */
		int compare_len; /* comparison length, only used if max == 0 */
		union
		{
			char* compare; /* Value to compare with */
			void* internal;
		};
	};

	int dump_nf_cts(bool expectations, struct cr_img* out);
	int restore_nf_cts(bool expectation, char* data, int data_len);
	void cr_free(cr_img* img);
	int cr_length(cr_node* node);
	
	void conditional_free();
	void conditional_init(int address_family, cr_filter* filters, int filters_len);
	bool conditional_filter(struct nlmsghdr *nlh);
}