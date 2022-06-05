#include "hdtp.h"

#include "proto.h"
#include "section.h"

#include "atom.h"
#include "clist.h"
#include "threading.h"
#include "avltree.h"
#include "zmalloc.h"

enum remote_host_type
{
    kRHT_IPC = 0,
    kRHT_UDP,
    kRHT_TC,
};

struct remote_host
{
    struct list_head entry_of_hosts;  /* entry of __server_context::head_of_hosts */
    struct avltree_node_t leaf_of_link_indexer;
    struct avltree_node_t leaf_of_name_indexer;
    union {
        HTCPLINK ipclink;
        HUDPLINK udpalias;
        HLNK link;
    };
    enum hdtp_link_type linktype;
    char name[128];
};

static struct {
    HTCPLINK ipclink;
    HTCPLINK udpserver;
    struct hdtp_callback_group callbacks;
    struct list_head head_of_hosts;
    int count_of_hosts;
    lwp_mutex_t mutex_lock_hosts;
    struct avltree_node_t *root_of_link_indexer;
    struct avltree_node_t *root_of_name_indexer;
} __server_context = {
    .ipclink = INVALID_HTCPLINK,
    .udpserver = INVALID_HUDPLINK,
    .callbacks = { NULL, NULL },
    .head_of_hosts = { &__server_context.head_of_hosts, &__server_context.head_of_hosts },
    .count_of_hosts = 0,
    .mutex_lock_hosts = { PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP },
    .root_of_link_indexer = NULL,
    .root_of_name_indexer = NULL,
};

static int _host_compare_by_link(const void *left, const void *right)
{
    return avl_type_compare(struct remote_host, leaf_of_link_indexer, link, left, right);
}

static int _host_compare_by_name(const void *left, const void *right)
{
    struct remote_host *lhost = container_of(left, struct remote_host, leaf_of_name_indexer);
    struct remote_host *rhost = container_of(right, struct remote_host, leaf_of_name_indexer);


    return strcasecmp(lhost->name, rhost->name);
}

static void _on_accepted(HTCPLINK server, HTCPLINK client)
{
    struct remote_host *host, find;
    struct avltree_node_t *found;

    host = (struct remote_host *)ztrymalloc(sizeof(*host));
    if (!host) {
        return;
    }

    host->ipclink = client;
    host->linktype = HDTP_LINK_IPC;

    find.link = client;
    lwp_mutex_lock(&__server_context.mutex_lock_hosts);
    found = avlsearch(__server_context.root_of_link_indexer, &find.leaf_of_link_indexer, &_host_compare_by_link);
    if (!found) {
        __server_context.root_of_link_indexer = avlinsert(__server_context.root_of_link_indexer, &host->leaf_of_link_indexer, &_host_compare_by_link);
        list_add_tail(&host->entry_of_hosts, &__server_context.head_of_hosts);
        __server_context.count_of_hosts++;
    }
    lwp_mutex_unlock(&__server_context.mutex_lock_hosts);

    if (found) {
        zfree(host);
    } else {
        nis_cntl(client, NI_SETCTX, host);
    }
}

static void _on_tcp_prclose(HTCPLINK link)
{
    struct remote_host *host, find;
    struct avltree_node_t *rmnode;

    find.ipclink = link;
    rmnode = NULL;
    lwp_mutex_lock(&__server_context.mutex_lock_hosts);
    __server_context.root_of_link_indexer = avlremove(__server_context.root_of_link_indexer, &find.leaf_of_link_indexer, &rmnode, &_host_compare_by_link);
    if (rmnode) {
        host = container_of(rmnode, struct remote_host, leaf_of_link_indexer);
        list_del(&host->entry_of_hosts);
        INIT_LIST_HEAD(&host->entry_of_hosts);
        __server_context.count_of_hosts--;
    }
    lwp_mutex_unlock(&__server_context.mutex_lock_hosts);

    if (rmnode) {
        host = container_of(rmnode, struct remote_host, leaf_of_link_indexer);
        if (__server_context.callbacks.on_closed) {
            __server_context.callbacks.on_closed(link, 0);
        }
        zfree(host);
    }
}

static void _on_tcp_closed(HTCPLINK link)
{

}

static void _on_tcp_logon(HTCPLINK link, struct hdtp_package *pkg)
{
    struct remote_host *host, find;
    struct avltree_node_t *found;
    const char *hostname;
    hdtp_uint16_t sectlen;

    hostname = NULL;
    sectlen = hdtp_query_section(pkg, kHDTP_INNER_SECTION_NAME, (const void **)&hostname);
    if (sectlen == 0 || !hostname) {
        return;
    }

    find.ipclink = link;
    lwp_mutex_lock(&__server_context.mutex_lock_hosts);
    found = avlsearch(__server_context.root_of_link_indexer, &find.leaf_of_link_indexer, &_host_compare_by_link);
    if (found) {
        host = container_of(found, struct remote_host, leaf_of_link_indexer);
        if (0 == host->name[0]) {
            strcpy(host->name, hostname);
            __server_context.root_of_name_indexer = avlinsert(__server_context.root_of_name_indexer, &host->leaf_of_name_indexer, &_host_compare_by_name);
        }
    }
    lwp_mutex_unlock(&__server_context.mutex_lock_hosts);
}

static void _on_tcp_received(HTCPLINK link, const unsigned char *data, int size)
{
    nsp_status_t status;
    struct hdtp_package *pkg;
    hdtp_uint16_t seq;
    hdtp_int32_t type;
    hdtp_int32_t error;

    status = build_received_package(data, size, &pkg);
    if (!NSP_SUCCESS(status)) {
        return;
    }

    status = query_package_base(pkg, &seq, &type, &error);
    if (!NSP_SUCCESS(status)) {
        return;
    }

    if (type == kHDTP_PROTO_LOGON) {
        _on_tcp_logon(link, pkg);
        return;
    }

    if (__server_context.callbacks.on_receviedata) {
        __server_context.callbacks.on_receviedata(link, seq, type, error, pkg);
    }
}

static void _tcp_callback(const struct nis_event *event, const void *data)
{
    struct nis_tcp_data *tcp_data = (struct nis_tcp_data *)data;

    switch (event->Event) {
        case EVT_RECEIVEDATA:
            _on_tcp_received(event->Ln.Tcp.Link, tcp_data->e.Packet.Data, tcp_data->e.Packet.Size);
            break;
        case EVT_TCP_ACCEPTED:
            _on_accepted(event->Ln.Tcp.Link, tcp_data->e.Accept.AcceptLink);
            break;
        case EVT_PRE_CLOSE:
            _on_tcp_prclose(event->Ln.Tcp.Link);
            break;
        case EVT_CLOSED:
            _on_tcp_closed(event->Ln.Tcp.Link);
            break;
        default:
            break;
    }
}

static void _on_udp_logon(HUDPLINK link, struct hdtp_package *pkg)
{
    struct remote_host *host;
    const char *hostname;
    struct avltree_node_t *found;
    hdtp_uint16_t sectlen;

    sectlen = hdtp_query_section(pkg, kHDTP_INNER_SECTION_NAME, (const void **)&hostname);
    if (sectlen == 0 ) {
        return;
    }

    host = (struct remote_host *)ztrymalloc(sizeof(*host));
    if (!host) {
        return;
    }
    strcpy(host->name, hostname);
    host->udpalias = objallo2(1);

    lwp_mutex_lock(&__server_context.mutex_lock_hosts);
    found = avlsearch(__server_context.root_of_name_indexer, &host->leaf_of_name_indexer, &_host_compare_by_name);
    if (!found) {
        __server_context.root_of_link_indexer = avlinsert(__server_context.root_of_link_indexer, &host->leaf_of_link_indexer, &_host_compare_by_link);
        __server_context.root_of_name_indexer = avlinsert(__server_context.root_of_name_indexer, &host->leaf_of_name_indexer, &_host_compare_by_name);
        list_add_tail(&host->entry_of_hosts, &__server_context.head_of_hosts);
    }
    lwp_mutex_unlock(&__server_context.mutex_lock_hosts);
}

static void _on_udp_received(HUDPLINK link, const unsigned char *data, int size, const char *rhost, uint16_t rport)
{
    nsp_status_t status;
    struct hdtp_package *pkg;
    hdtp_uint16_t seq;
    hdtp_int32_t type;
    hdtp_int32_t error;

    status = build_received_package(data, size, &pkg);
    if (!NSP_SUCCESS(status)) {
        return;
    }

    status = query_package_base(pkg, &seq, &type, &error);
    if (!NSP_SUCCESS(status)) {
        return;
    }

    if (type == kHDTP_PROTO_LOGON) {
        _on_udp_logon(link, pkg);
        return;
    }

    if (__server_context.callbacks.on_receviedata) {
        __server_context.callbacks.on_receviedata(link, seq, type, error, pkg);
    }
}

static void _udp_callback(const struct nis_event *event, const void *data)
{
    struct nis_udp_data *udp_data = (struct nis_udp_data *)data;

    switch (event->Event) {
        case EVT_RECEIVEDATA:
            _on_udp_received(event->Ln.Udp.Link, udp_data->e.Packet.Data, udp_data->e.Packet.Size, udp_data->e.Packet.RemoteAddress, udp_data->e.Packet.RemotePort);
            break;
        case EVT_PRE_CLOSE:
        case EVT_CLOSED:
            break;
        default:
            break;
    }
}

hdtp_status_t hdtp_create_server(const struct hdtp_endpoint_v4 *udpserver, const struct hdtp_ipc_domain *ipcdomain,
    const struct hdtp_callback_group *callbacks)
{
    HLNK expect;
    HTCPLINK ipclink;
    HUDPLINK udplink;
    char ipcstr[108];
    int attr;

    if (!callbacks) {
        return posix__makeerror(EINVAL);
    }

    tcp_init2(4);
    udp_init2(2);

    /* create ipc server */
    if (ipcdomain) {
        snprintf(ipcstr, sizeof(ipcstr), "ipc:%s", ipcdomain->file);
        ipclink = tcp_create2(&_tcp_callback, ipcstr, 0, gettst());
        if (INVALID_HTCPLINK != ipclink) {
            expect = INVALID_HTCPLINK;
            if (!atom_compare_exchange_strong(&__server_context.ipclink, &expect, ipclink)) {
                tcp_destroy(ipclink);
            } else {
                attr = nis_cntl(ipclink, NI_GETATTR);
                if (attr >= 0 ) {
                    attr |= LINKATTR_TCP_UPDATE_ACCEPT_CONTEXT;
                    attr |= LINKATTR_TCP_NO_BUILD;
                    attr = nis_cntl(ipclink, NI_SETATTR, attr);
                }
                tcp_listen(ipclink, 10);
            }
        }
    }

    if (udpserver) {
        udplink = udp_create(&_udp_callback,
                                ((NULL == udpserver) ? NULL : ((NULL == udpserver->ipstrptr) ? NULL : udpserver->ipv4str)),
                                (NULL == udpserver) ? 0 : udpserver->port,
                                0);
        if (INVALID_HUDPLINK != udplink) {
            expect = INVALID_HUDPLINK;
            if (!atom_compare_exchange_strong(&__server_context.udpserver, &expect, udplink)) {
                udp_destroy(udplink);
            }
        }
    }

    if (INVALID_HTCPLINK == __server_context.ipclink && INVALID_HUDPLINK == __server_context.udpserver) {
        return HDTP_STATUS_FATAL;
    } else {
        memcpy(&__server_context.callbacks, callbacks, sizeof(__server_context.callbacks));
    }

    return HDTP_STATUS_SUCCESSFUL;
}
