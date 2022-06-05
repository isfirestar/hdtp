#if !defined (HIK_DAEMON_TRANSFER_PROTOCOL_HEADER)
#define HIK_DAEMON_TRANSFER_PROTOCOL_HEADER

typedef struct hdtp_section *hdtp_result_pt;
typedef struct hdtp_section *hdtp_section_pt;
typedef long LINK;
typedef int hdtp_status_t;

typedef void (*on_receivedata_t)(LINK from, unsigned short seq, unsigned short type, int error, const struct hdtp_result_pt datares);
typedef void (*on_closed_t)(LINK from, int error);

struct hdtp_callback_group
{
	on_receivedata_t	on_receviedata;
	on_closed_t			on_closed;
};

struct hdtp_endpoint_v4
{
	char ipv4str[16];
	unsigned short port;
};

extern hdtp_status_t hdtp_create_server(const struct hdtp_endpoint_v4 *tcpserver, const char *ipcfile, const struct hdtp_callback_group *callbacks);
extern hdtp_status_t hdtp_create_client(const struct hdtp_endpoint_v4 *tcpclient, const char *ipcfile, const struct hdtp_callback_group *callbacks, LINK *link);
extern hdtp_status_t hdtp_write_data(LINK from, const hdtp_section_pt section);

/* @hdtp_allocate_section: on a initiative request step, calling thread shall allocate a section object to build HDTP package for send
 * ignore parameter @error when you're a requester
 * ignore parameter @seq when you didn't want to follow the transfer flow   
 */
extern hdtp_status_t hdtp_allocate_section(unsigned short seq, unsigned short type, int error, hdtp_section_pt **section);

/* @hdtp_append_section function use to append data upon a specify protocol section pointer to by @section
 *   @categroy and @sectlen are mandatory parameters that calling thread MUST explicit tell the interface
 *	 @section is the section pointer which allocate by successful call to @hdtp_allocate_section procedure.
 * 	 @buff is pointer to the data which you need to storage on this section. data length MUST match the len which specified by @sectlen.
 *	 @buff CAN be NULL, in this case, behavior of interface shall be change to reserve continue memory with @sectlen bytes for the section, calling thread allowed update memory data by call @hdtp_update_section  
 */
extern hdtp_status_t hdtp_append_section(unsigned short categroy, const unsigned char *buff, const unsigned short sectlen, hdtp_section_pt section);
extern hdtp_status_t hdtp_update_section(unsigned short categroy, const unsigned char *buff, const unsigned short bufflen, hdtp_section_pt section);
#define hdtp_append_value(categroy, value, section) hdtp_append_section(category, &(value), sizeof(value), section)
#define hdtp_append_valueptr(categroy, valueptr, section) hdtp_append_section(category, (value), sizeof(*(value)), section)
extern hdtp_status_t hdtp_duplicate_section(unsigned short categroy, const hdtp_section_pt orig, hdtp_section_pt dest);

/* free the section pointer where allocate by successful call to @hdtp_append_section 
 */
extern void hdtp_free_section(hdtp_section_pt section);

/* @hdtp_query_section method use to query section from result set
 *	on successful, return value indicate length of bytes in @sectdata, otherwise, return 0 indicate error maybe follow reason:
 *		1. @categroy does not exist in this section
 *		2. one or more input parameter illegal
 */
extern unsigned short hdtp_query_section(const struct hdtp_result_pt datares, unsigned short categroy, void **sectdata);

#endif HIK_DAEMON_TRANSFER_PROTOCOL_HEADER /* !HIK_DAEMON_TRANSFER_PROTOCOL_HEADER */
