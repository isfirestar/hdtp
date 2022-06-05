#if !defined (HIK_DAEMON_TRANSFER_PROTOCOL_HEADER)
#define HIK_DAEMON_TRANSFER_PROTOCOL_HEADER

#if _HDTP_SECTION_DECLARE
typedef struct hdtp_package *hdtp_package_pt;
#else
typedef void *hdtp_package_pt;
#endif

typedef long HDTPLINK;
typedef int hdtp_status_t;
typedef unsigned short hdtp_uint16_t;
typedef unsigned int hdtp_uint32_t;
typedef int hdtp_int32_t;
typedef void (*on_receivedata_t)(HDTPLINK from, hdtp_uint16_t seq, hdtp_int32_t type, hdtp_int32_t error, const hdtp_package_pt package);
typedef void (*on_closed_t)(HDTPLINK from, int error);

#define HDTP_STATUS_SUCCESSFUL          (0)
#define HDTP_STATUS_FATAL               ( ~((hdtp_status_t)(0)) )
#define HDTP_SUCCESS(status)            ((status) >= HDTP_STATUS_SUCCESSFUL)
#define TCP_USER_LAYER_MTU              (1460)
#define MAXIMUM_PACKAGE_SIZE_PRESET     (TCP_USER_LAYER_MTU)

struct hdtp_callback_group
{
	on_receivedata_t	on_receviedata;
	on_closed_t			on_closed;
};

struct hdtp_endpoint_v4
{
    union {
        char ipv4str[16];   /* dotta-decimal represent v4 IP address, prior lower than @ipstrptr */
        char *ipstrptr;     /* set this pointer to NULL meats use random local address "0.0.0.0" */
    };
	hdtp_uint16_t port;    /* zero port indicate a system auto select access */
};

struct hdtp_ipc_domain
{
    char file[102];  /* restrict by sockaddr_un::sun_path */
};

enum hdtp_link_type
{
    HDTP_LINK_UNKNOWN = 0,
    HDTP_LINK_IPC,
    HDTP_LINK_TCP,
    HDTP_LINK_UDP,
    HDTP_LINK_MAXIMUM_FUNCTION,
};

/* server creation impls */
extern hdtp_status_t hdtp_create_server(const struct hdtp_endpoint_v4 *udpserver, const struct hdtp_ipc_domain *ipcdomain,
    const struct hdtp_callback_group *callbacks);
extern void hdtp_close_server();

/* client creation impls */
extern hdtp_status_t hdtp_create_ipc_client(const struct hdtp_ipc_domain *ipcdomain, const char *name,
    const struct hdtp_callback_group *callbacks, HDTPLINK *ipclink);
extern hdtp_status_t hdtp_create_udp_client(const struct hdtp_endpoint_v4 *udpclient, const struct hdtp_endpoint_v4 *udpserver, const char *name,
    const struct hdtp_callback_group *callbacks, HDTPLINK *udplink);

/* below pair functions use to search a link which represent a process and send a package data to it
 * @pacakge MUST be assigne and build by using @hdtp_allocate_package.
 * calling thread can append user section upon package by interface @hdtp_append_section
 */
extern HDTPLINK hdtp_query_process_link(const char *psname);
extern hdtp_status_t hdtp_write(HDTPLINK link, const hdtp_package_pt package);

/* caller can easy to get link type if you want :)
 * but when you calling this procedure, pls think about why you have to do it? :(
 */
extern enum hdtp_link_type hdtp_query_linktype(HDTPLINK link);

/* @hdtp_allocate_package: on a initiative request step, calling thread shall allocate a object associated HDTP package for send
 *  ignore parameter @error when you're a requester
 *  ignore parameter @seq when you didn't want to follow the transfer flow
 * @hdtp_free_package MUST explicit call to release memory which assigned by @hdtp_allocate_package
 */
extern hdtp_status_t hdtp_allocate_package(hdtp_uint16_t seq, hdtp_uint16_t type, int error, hdtp_package_pt *package);
extern void hdtp_free_package(hdtp_package_pt package);

/* @hdtp_append_section function use to append data to a specify package pointer to by @package
 *   @categroy and @length are mandatory parameters that calling thread MUST explicit tell the interface
 *	 @package is the package pointer which allocate by successful call to @hdtp_allocate_package procedure.
 * 	 @buff is pointer to the data which you need to storage on this section. data length MUST match the len which specified by @length.
 *	 @buff CAN be NULL, in this case, behavior of interface shall be change to reserve continue memory with @length bytes for the section,
 *      calling thread allowed update memory data by call @hdtp_update_section
 */
extern hdtp_status_t hdtp_append_section(hdtp_uint16_t categroy, const unsigned char *buff, hdtp_uint16_t length, hdtp_package_pt package);
extern hdtp_status_t hdtp_update_section(hdtp_uint16_t categroy, const unsigned char *buff, hdtp_uint16_t offset, hdtp_uint16_t length, hdtp_package_pt package);
#define hdtp_append_value(categroy, value, package) hdtp_append_section(categroy, (const unsigned char *)(&(value)), sizeof(value), (package))
#define hdtp_append_valueptr(categroy, valueptr, package) hdtp_append_section(categroy, (valueptr), sizeof(*(valueptr)), (package))
#define hdtp_append_string(categroy, string, package) hdtp_append_section(categroy, (const unsigned char *)(string), strlen((string)) + 1, (package))

/* @hdtp_query_section interface use to query section from package
 *	on successful, return value indicate length of bytes in @sectdata, otherwise, return 0 indicate error maybe follow reason:
 *		1. @categroy does not exist in this package
 *		2. one or more input parameter illegal
 */
extern hdtp_uint16_t hdtp_query_section(const hdtp_package_pt package, hdtp_uint16_t categroy, const void **sectdata);

#endif /* !HIK_DAEMON_TRANSFER_PROTOCOL_HEADER */
