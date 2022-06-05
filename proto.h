#if !defined (HDTP_PROTOCOL_HEADER)
#define HDTP_PROTOCOL_HEADER

#include "hdtp.h"

#include "nis.h"

#pragma pack(push, 1)

struct hdtp_lowlevel_head
{
    hdtp_uint32_t   opcode;
    hdtp_uint16_t   len;    /* total data length exclude sizeof(struct hdtp_head) */
};

struct hdtp_app_head
{
    hdtp_uint16_t   seq;
    hdtp_int32_t    type;
    hdtp_int32_t    err;
    unsigned char   payload[0];
};

struct hdtp_section_head
{
    hdtp_uint16_t   categroy;
    hdtp_uint16_t   len;    /* total data length exclude sizeof(struct hdtp_section_head) */
    unsigned char   payload[0];
};

#pragma pack(pop)

extern const unsigned char HDTP_OPCODE[4];
extern const tst_t *gettst();

enum hdtp_inner_proto
{
    kHDTP_PROTO_LOGON = 0,
};

#endif /* HDTP_PROTOCOL_HEADER */
