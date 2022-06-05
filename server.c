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
    kRHT_TCP = 1;
};

struct remote_host
{
    struct list_head entry;  /* entry of __server_context::head_of_ps */
    struct avltree_node_t leaf_of_link_indexer;
    HTCPLINK link;
    enum hdtp_link_type linktype;

    union {
        struct {
            pid_t pid;
            char psname[108];
        } ipc;

        struct {

        } tcp;
    } u;
};

static struct {
    HTCPLINK ipclink;
    HTCPLINK tcplink;
    struct hdtp_callback_group callbacks;
    struct list_head head;
    int count_of_hosts;
    lwp_mutex_t mutex_lock_hosts;
    struct avltree_node_t *root_of_link_indexer;
} __server_context = {
    .ipclink = INVALID_HTCPLINK,
    .tcplink = INVALID_HTCPLINK,
    .callbacks = { NULL, NULL },
    .head = { &__server_context.head, &__server_context.head },
    .count_of_hosts = 0,
    .mutex_lock_hosts = { PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP },
    .root_of_link_indexer = NULL,
};

static int host_compare_by_link(const void *left, const void *right)
{
    return avl_type_compare(struct remote_host, leaf_of_link_indexer, link, left, right);
}

static void on_accepted(HTCPLINK server, HTCPLINK client)
{
    struct remote_host *host, find;
    struct avltree_node_t *found;

    host = (struct remote_host *)ztrymalloc(sizeof(*host));
    if (!host) {
        return;
    }

    host->link = client;
    host->linktype = ((server == __server_context.ipclink) ? HDTP_LINK_IPC : HDTP_LINK_TCP);

    find.link = client;
    lwp_mutex_lock(&__server_context.mutex_lock_hosts);
    found = avlsearch(__server_context.root_of_link_indexer, &find.leaf_of_link_indexer, &host_compare_by_link);
    if (!found) {
        __server_context.root_of_link_indexer = avlinsert(__server_context.root_of_link_indexer, &host->leaf_of_link_indexer, &host_compare_by_link);
        list_add_tail(&host->entry, &__server_context.head);
        __server_context.count_of_hosts++;
    }
    lwp_mutex_unlock(&__server_context.mutex_lock_hosts);

    if (found) {
        zfree(host);
    } else {
        nis_cntl(client, NI_SETCTX, host);
    }
}

static void on_pre_close(HTCPLINK link)
{
    struct remote_host *host, find;
    struct avltree_node_t *rmnode;

    find.link = link;
    rmnode = NULL;
    lwp_mutex_lock(&__server_context.mutex_lock_hosts);
    __server_context.root_of_link_indexer = avlremove(__server_context.root_of_link_indexer, &find.leaf_of_link_indexer, &rmnode, &host_compare_by_link);
    if (rmnode) {
        host = container_of(rmnode, struct remote_host, leaf_of_link_indexer);
        list_del(&host->entry);
        INIT_LIST_HEAD(&host->entry);
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

static void on_closed(HTCPLINK link)
{

}

static void on_logon(HTCPLINK link, const struct hdtp_package *pkg);
static void on_received(HTCPLINK link, const unsigned char *data, int size)
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
        on_logon(link, pkg);
        return;
    }

    if (__server_context.callbacks.on_receviedata) {
        __server_context.callbacks.on_receviedata(link, seq, type, error, pkg);
    }
}

static void tcp_callback(const struct nis_event *event, const void *data)
{
    struct nis_tcp_data *tcp_data = (struct nis_tcp_data *)data;

    switch (event->Event) {
        case EVT_RECEIVEDATA:
            on_received(event->Ln.Tcp.Link, tcp_data->e.Packet.Data, tcp_data->e.Packet.Size);
            break;
        case EVT_TCP_ACCEPTED:
            on_accepted(event->Ln.Tcp.Link, tcp_data->e.Accept.AcceptLink);
            break;
        case EVT_PRE_CLOSE:
            on_pre_close(event->Ln.Tcp.Link);
            break;
        case EVT_CLOSED:
            on_closed(event->Ln.Tcp.Link);
            break;
        default:
            break;
    }
}

static void on_logon(HTCPLINK link, struct hdtp_package *pkg)
{

}

hdtp_status_t hdtp_create_server(const struct hdtp_endpoint_v4 *tcpserver, const struct hdtp_ipc_domain *ipcdomain,
    const struct hdtp_callback_group *callbacks)
{
    HTCPLINK ipclink, tcplink, expect;
    char ipcstr[108];
    int attr;

    tcp_init2(4);

    /* create ipc server */
    if (ipcdomain) {
        snprintf(ipcstr, sizeof(ipcstr), "ipc:%s", ipcdomain->file);
        ipclink = tcp_create2(&tcp_callback, ipcstr, 0, gettst());
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

    if (tcpserver) {
        tcplink = tcp_create2(&tcp_callback, ((tcpserver->ipstrptr == NULL) ? NULL : (tcpserver->ipv4str)), tcpserver->port, gettst());
        if (INVALID_HTCPLINK != tcplink) {
            expect = INVALID_HTCPLINK;
            if (!atom_compare_exchange_strong(&__server_context.tcplink, &expect, tcplink)) {
                tcp_destroy(tcplink);
            } else {
                attr = nis_cntl(tcplink, NI_GETATTR);
                if (attr >= 0 ) {
                    attr |= LINKATTR_TCP_UPDATE_ACCEPT_CONTEXT;
                    attr |= LINKATTR_TCP_NO_BUILD;
                    attr = nis_cntl(tcplink, NI_SETATTR, attr);
                }
                tcp_listen(tcplink, 10);
            }
        }
    }

    if (callbacks) {
        memcpy(&__server_context.callbacks, callbacks, sizeof(__server_context.callbacks));
    }

    if (INVALID_HTCPLINK == __server_context.ipclink && INVALID_HTCPLINK == __server_context.tcplink) {
        return HDTP_STATUS_FATAL;
    }

    return HDTP_STATUS_SUCCESSFUL;
}
