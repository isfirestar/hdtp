#include "hdtp.h"

#include "compiler.h"
#include "threading.h"

#include <stdio.h>

#define PKTTYPE_CREATOR_REPORT_FAULT_TO_DAEMON  (0x100)

#define CATEGROY_CREATOR_FAULT_CODE             (0x100)
#define CATEGROY_CREATOR_FAULT_REASON           (0x101)

void demo_on_receviedata(HDTPLINK from, hdtp_uint16_t seq, hdtp_int32_t type, hdtp_int32_t error, const hdtp_package_pt package)
{
    hdtp_uint16_t sectlen;
    const int *fault_code;
    const char *pmsg;

    printf("receive package: seq:%u, type:0x%04x, error:%u\n", seq, type, error);

    sectlen = hdtp_query_section(package, CATEGROY_CREATOR_FAULT_CODE, NULL);
    if (4 == sectlen) {
        hdtp_query_section(package, CATEGROY_CREATOR_FAULT_CODE, (const void **)&fault_code);
        printf("fault code:%d\n", *fault_code);
    }

    sectlen = hdtp_query_section(package, CATEGROY_CREATOR_FAULT_REASON, (const void **)&pmsg);
    if (0 != sectlen) {
        printf("fault message:%s\n", pmsg);
    }
}

void demo_on_closed(HDTPLINK from, int error)
{

}

int main(int argc, char **argv)
{
    struct hdtp_ipc_domain ipcdomain;
    struct hdtp_callback_group callback;
    hdtp_status_t status;

    callback.on_receviedata = &demo_on_receviedata;
    callback.on_closed = &demo_on_closed;
    strcpy(ipcdomain.file, "/dev/shm/daemon.sock");
    status = hdtp_create_server(NULL, &ipcdomain, &callback);
    if (!HDTP_SUCCESS(status)) {
        return 1;
    }

    lwp_hang();
    return 0;
}
