#include "proto.h"

const unsigned char HDTP_OPCODE[4] = { 'H', 'd', 't', 'p' };

static nsp_status_t STDCALL hdtp_tst_parser(void *dat, int cb, int *pkt_cb)
{
    struct hdtp_lowlevel_head *head = (struct hdtp_lowlevel_head *)dat;

    if (!head) {
        return posix__makeerror(EINVAL);
    }

    if (0 != memcmp(HDTP_OPCODE, &head->opcode, sizeof(HDTP_OPCODE))) {
        return NSP_STATUS_FATAL;
    }

    *pkt_cb = head->len + sizeof(struct hdtp_app_head);
    return NSP_STATUS_SUCCESSFUL;
}

static nsp_status_t STDCALL hdtp_tst_builder(void *dat, int cb)
{
    struct hdtp_lowlevel_head *head = (struct hdtp_lowlevel_head *)dat;

    if (!dat || cb <= 0) {
        return posix__makeerror(EINVAL);
    }

    memcpy(&head->opcode, HDTP_OPCODE, sizeof(HDTP_OPCODE));
    head->len = cb;
    return NSP_STATUS_SUCCESSFUL;
}

const tst_t *gettst()
{
    /* nail the TST and then create TCP link */
    static const tst_t tst = {
        .parser_ = &hdtp_tst_parser,
        .builder_ = &hdtp_tst_builder,
        .cb_ = sizeof(struct hdtp_lowlevel_head),
    };
    return &tst;
}
