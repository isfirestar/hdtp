#include "hdtp.h"

#include "compiler.h"
#include "threading.h"

void demo_on_receviedata(HDTPLINK from, hdtp_uint16_t seq, hdtp_int32_t type, hdtp_int32_t error, const hdtp_package_pt package)
{

}

void demo_on_closed(HDTPLINK from, int error)
{

}

#define PKTTYPE_CREATOR_REPORT_FAULT_TO_DAEMON  (0x100)

#define CATEGROY_CREATOR_FAULT_CODE             (0x100)
#define CATEGROY_CREATOR_FAULT_REASON           (0x101)

int main(int argc, char **argv)
{
    struct hdtp_ipc_domain ipcdomain;
    struct hdtp_callback_group callback;
    HDTPLINK link;
    hdtp_status_t status;
    hdtp_package_pt package;
    int test_error_code;

    callback.on_receviedata = &demo_on_receviedata;
    callback.on_closed = &demo_on_closed;
    strcpy(ipcdomain.file, "/dev/shm/daemon.sock");
    status = hdtp_create_client(NULL, NULL, &ipcdomain, &callback, &link);
    if (!HDTP_SUCCESS(status)) {
        return 1;
    }

    status = hdtp_allocate_package(1, PKTTYPE_CREATOR_REPORT_FAULT_TO_DAEMON, 0, &package);
    if (!HDTP_SUCCESS(status)) {
        test_error_code = 10086;
        hdtp_append_value(CATEGROY_CREATOR_FAULT_CODE, test_error_code, package);
        hdtp_append_string(CATEGROY_CREATOR_FAULT_REASON, "memory leak", package);

        /*   0x55555579ce78: 0x48    0x64    0x74    0x70    0x18    0x00    0x01    0x00    OP='Hdtp' LEN=24 SEQ=1
         *   0x55555579ce80: 0x00    0x01    0x00    0x00    0x00    0x00    0x00    0x00    TYPE=0x100 ERR=0
         *   0x55555579ce88: 0x00    0x01    0x04    0x00    0x66    0x27    0x00    0x00    SECTION : CATEGROY=0x100 LEN=4 VALUE=0x2766(10086)
         *   0x55555579ce90: 0x01    0x01    0x0c    0x00    0x6d    0x65    0x6d    0x6f    SECTION : CATEGROY=0x101 LEN=12 VALUE="memo
         *   0x55555579ce98: 0x72    0x79    0x20    0x6c    0x65    0x61    0x6b    0x00    ry leak\0"
        */
        hdtp_write(link, package);
        hdtp_free_package(package);
    }

    lwp_hang();
    return 0;
}
