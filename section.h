#if !defined (HDTP_SECTION_HEADER)
#define HDTP_SECTION_HEADER

#if !defined _HDTP_SECTION_DECLARE
#define _HDTP_SECTION_DECLARE   1
#endif

#include "hdtp.h"
#include "proto.h"
#include "avltree.h"
#include "clist.h"

struct hdtp_package;
struct hdtp_section
{

    struct avltree_node_t leaf_of_sections;
    struct list_head entry_of_sections;
    struct hdtp_package *package;
    hdtp_uint16_t   categroy;
    const struct hdtp_section_head *head;
};

struct hdtp_package
{
    struct hdtp_lowlevel_head   *low_head;
    struct hdtp_app_head        *app_head;

    union {
        struct {
            unsigned char raw[TCP_USER_LAYER_MTU];
        } tx;

        struct {
            const unsigned char *source;
            unsigned short size;
            struct avltree_node_t *root_of_sections;
            unsigned short count_of_sections;
            struct list_head head_of_sections;
        } rx;
    } u;
};

enum hdtp_inner_proto_type
{
    kHIPT_LOGON = 0,
};

enum hdtp_inner_section_type
{
    kHDTP_INNER_SECTION_PID = 0,
    kHDTP_INNER_SECTION_NAME,
};

extern const hdtp_uint16_t HDTP_BASE_HEAD_LEN;

extern nsp_status_t build_received_package(const unsigned char *data, unsigned short size, struct hdtp_package **package);
extern nsp_status_t query_package_base(const hdtp_package_pt package, hdtp_uint16_t *seq, hdtp_int32_t *type, hdtp_int32_t *error);

#endif
