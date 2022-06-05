#include "hdtp.h"

#include "nis.h"
#include "atom.h"

#include "proto.h"
#include "section.h"

static struct {
    HTCPLINK ipclink;
    HTCPLINK tcplink;
    HUDPLINK udplink;
    struct hdtp_endpoint_v4 udpserver;
    struct hdtp_callback_group callbacks;
    struct hdtp_endpoint_v4 tcpserver;
    struct hdtp_endpoint_v4 tcpclient;
} __client_context = {
    .ipclink = INVALID_HTCPLINK,
    .tcplink = INVALID_HTCPLINK,
    .udplink = INVALID_HUDPLINK,
    .udpserver = { .ipstrptr = NULL, .port = 0 },
    .callbacks = { NULL, NULL },
};

static void _on_client_pre_close(HTCPLINK link)
{

}

static void _on_client_closed(HTCPLINK link)
{

}

static void _on_received(HTCPLINK link, const unsigned char *data, int size)
{
    nsp_status_t status;
    struct hdtp_package *pkg;
    hdtp_uint16_t seq;
    hdtp_int32_t type;
    hdtp_int32_t error;

    if (!__client_context.callbacks.on_receviedata) {
        return;
    }

    status = build_received_package(data, size, &pkg);
    if (!NSP_SUCCESS(status)) {
        return;
    }

    status = query_package_base(pkg, &seq, &type, &error);
    if (!NSP_SUCCESS(status)) {
        return;
    }

    __client_context.callbacks.on_receviedata(link, seq, type, error, pkg);
}

static void _on_udp_received(HUDPLINK link, const unsigned char *data, int size, const char *rhost, unsigned short rport)
{

}

static void _client_callback(const struct nis_event *event, const void *data)
{
    tcp_data_t *tcp_data = (tcp_data_t *)data;
    udp_data_t *udp_data = (udp_data_t *)data;

    switch (event->Event) {
        case EVT_RECEIVEDATA:
            if (event->Ln.Tcp.Link == __client_context.ipclink || event->Ln.Tcp.Link == __client_context.tcplink) {
                _on_received(event->Ln.Tcp.Link, tcp_data->e.Packet.Data, tcp_data->e.Packet.Size);
            } else {
                if (event->Ln.Udp.Link == __client_context.udplink) {
                    _on_udp_received(event->Ln.Udp.Link, udp_data->e.Packet.Data, udp_data->e.Packet.Size,
                        udp_data->e.Packet.RemoteAddress, udp_data->e.Packet.RemotePort);
                }
            }

            break;
        case EVT_PRE_CLOSE:
            _on_client_pre_close(event->Ln.Tcp.Link);
            break;
        case EVT_CLOSED:
            _on_client_closed(event->Ln.Tcp.Link);
            break;
        default:
            break;
    }
}

hdtp_status_t hdtp_create_ipc_client(const struct hdtp_ipc_domain *ipcdomain,
    const struct hdtp_callback_group *callbacks, HDTPLINK *outputlink)
{
    HTCPLINK ipclink, expect;
    char ipcstr[108];
    nsp_status_t status;
    int attr;

    tcp_init2(2);

    if (ipcdomain) {
        return posix__makeerror(EINVAL);
    }

    status = NSP_STATUS_FATAL;
    /* create ipc client */
    ipclink = tcp_create2(&_client_callback, "ipc:", 0, gettst());
    do {
        if (INVALID_HTCPLINK == ipclink) {
            break;
        }

        expect = INVALID_HTCPLINK;
        if (!atom_compare_exchange_strong(&__client_context.ipclink, &expect, ipclink)) {
            tcp_destroy(ipclink);
            break;
        }

        attr = nis_cntl(ipclink, NI_GETATTR);
        if (attr >= 0 ) {
            attr |= LINKATTR_TCP_NO_BUILD;
            attr = nis_cntl(ipclink, NI_SETATTR, attr);
        }

        snprintf(ipcstr, sizeof(ipcstr), "ipc:%s", ipcdomain->file);
        status = tcp_connect(ipclink, ipcstr, 0);
        if (!NSP_SUCCESS(status)) {
            tcp_destroy(ipclink);
            atom_exchange(&__client_context.ipclink, INVALID_HTCPLINK);
            break;
        }

        if (outputlink) {
            *outputlink = ipclink;
        }

        status = NSP_STATUS_SUCCESSFUL;
    } while(0);

    if (HDTP_SUCCESS(status)) {
        if (callbacks) {
            memcpy(&__client_context.callbacks, callbacks, sizeof(__client_context.callbacks));
        }
    }

    return status;
}

hdtp_status_t hdtp_create_udp_client(const struct hdtp_endpoint_v4 *udpclient, const struct hdtp_endpoint_v4 *udpserver,
    const struct hdtp_callback_group *callbacks, HDTPLINK *outputlink)
{
    HUDPLINK udplink, expect;
    nsp_status_t status;

    udp_init(2);

    if (!udpserver) {
        return posix__makeerror(EINVAL);
    }

    status = NSP_STATUS_FATAL;
    udplink = udp_create(&_client_callback,
        ((NULL == udpclient) ? NULL : ((NULL == udpclient->ipstrptr) ? NULL : udpclient->ipv4str)),
        (NULL == udpclient) ? 0 : udpclient->port,
        0);
    do {
        if (INVALID_HUDPLINK == udplink) {
            break;
        }

        expect = INVALID_HUDPLINK;
        if (!atom_compare_exchange_strong(&__client_context.udplink, &expect, udplink)) {
            udp_destroy(udplink);
            break;
        }

        if (outputlink) {
            *outputlink = udplink;
        }

        status = NSP_STATUS_SUCCESSFUL;
    } while(0);

    if (HDTP_SUCCESS(status)) {
        if (callbacks) {
            memcpy(&__client_context.callbacks, callbacks, sizeof(__client_context.callbacks));
        }
        memcpy(&__client_context.udpserver, udpserver, sizeof(*udpserver));
    }

    return status;
}
