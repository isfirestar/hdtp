#include "section.h"

#include "zmalloc.h"

const hdtp_uint16_t HDTP_BASE_HEAD_LEN = sizeof(struct hdtp_lowlevel_head ) + sizeof(struct hdtp_app_head);

hdtp_status_t hdtp_allocate_package(hdtp_uint16_t seq, hdtp_uint16_t type, int error, hdtp_package_pt *package)
{
    struct hdtp_package *pkg;

    pkg = (struct hdtp_package *)ztrymalloc(sizeof(*pkg));
    if (!pkg) {
        return posix__makeerror(ENOMEM);
    }

    pkg->low_head = (struct hdtp_lowlevel_head *)(pkg->u.tx.raw);
    memcpy(&pkg->low_head->opcode, HDTP_OPCODE, sizeof(pkg->low_head->opcode));
    pkg->low_head->len = 0;

    pkg->app_head = (struct hdtp_app_head *)(pkg->u.tx.raw + sizeof(*pkg->low_head));
    pkg->app_head->seq = seq;
    pkg->app_head->type = type;
    pkg->app_head->err = error;
    *package = pkg;
    return HDTP_STATUS_SUCCESSFUL;
}

void hdtp_free_package(hdtp_package_pt package)
{
    if (package) {
        zfree(package);
    }
}

hdtp_status_t hdtp_append_section(hdtp_uint16_t categroy, const unsigned char *buff, hdtp_uint16_t length, hdtp_package_pt package)
{
    struct hdtp_section_head *section;

    if (!package || 0 == length) {
        return posix__makeerror(EINVAL);
    }

    if (package->low_head->len + length > sizeof(package->u.tx.raw)) {
        return posix__makeerror(EINVAL);
    }

    section = (struct hdtp_section_head *)((unsigned char *)package->u.tx.raw + HDTP_BASE_HEAD_LEN + package->low_head->len);
    section->categroy = categroy;
    section->len = length;
    if (buff) {
        memcpy(section->payload, buff, length);
    }

    /* increase package length */
    package->low_head->len += (sizeof(struct hdtp_section_head) + length);
    return HDTP_STATUS_SUCCESSFUL;
}

hdtp_status_t hdtp_update_section(hdtp_uint16_t categroy, const unsigned char *buff, hdtp_uint16_t offset, hdtp_uint16_t length, hdtp_package_pt package)
{
    struct hdtp_section_head *section;
    hdtp_uint16_t remain;
    unsigned char *offtag;
    hdtp_uint16_t step;

    if (!package || 0 == length || !buff) {
        return posix__makeerror(EINVAL);
    }

    offtag = (unsigned char *)package->u.tx.raw + HDTP_BASE_HEAD_LEN;
    remain = package->low_head->len;
    while (remain > 0) {
        section = (struct hdtp_section_head *)offtag;
        if (section->categroy == categroy) {
            if (offset + length >= section->len) {
                 return posix__makeerror(EINVAL);
            }
            memcpy(section->payload + offset, buff, length);
            return HDTP_STATUS_SUCCESSFUL;
        }

        /* move cursor to next section object */
        step = (sizeof(struct hdtp_section_head) + section->len);
        remain -= step;
        offtag += step;
    }

    return posix__makeerror(ENOENT);
}

static int section_compare_by_categroy(const void *left, const void *right)
{
    struct hdtp_section *lsect, *rsect;

    lsect = container_of(left, struct hdtp_section, leaf_of_sections );
    rsect = container_of(right, struct hdtp_section, leaf_of_sections );

    if (lsect->categroy > rsect->categroy) {
        return 1;
    }

    if (lsect->categroy < rsect->categroy) {
        return -1;
    }

    return 0;
}

nsp_status_t build_received_package(const unsigned char *data, unsigned short size, struct hdtp_package **package)
{
    struct hdtp_package *pkg;
    unsigned short remain, offset;
    struct hdtp_section *section;
    nsp_status_t status;

    if (!data || 0 == size || !package) {
        return posix__makeerror(EINVAL);
    }
    remain = size;
    offset = 0;

    pkg = (struct hdtp_package *)ztrymalloc(sizeof(*pkg));
    if (!pkg) {
        return posix__makeerror(ENOMEM);
    }
    pkg->u.rx.source = data;
    pkg->u.rx.size = size;
    pkg->u.rx.root_of_sections = NULL;
    pkg->u.rx.count_of_sections = 0;
    INIT_LIST_HEAD(&pkg->u.rx.head_of_sections);
    pkg->app_head = (struct hdtp_app_head *)data;

    /* there no sections upon this package */
    remain -= sizeof(struct hdtp_app_head);
    if (0 == remain) {
        return HDTP_STATUS_SUCCESSFUL;
    }
    offset += sizeof(struct hdtp_app_head);

    status = NSP_STATUS_SUCCESSFUL;
    while (remain > 0) {
        section = (struct hdtp_section *)ztrymalloc(sizeof(*section));
        if (!section) {
            status = posix__makeerror(ENOMEM);
            break;
        }
        section->head = (const struct hdtp_section_head *)(data + offset);
        section->categroy = section->head->categroy;
        section->package = pkg;
        pkg->u.rx.root_of_sections = avlinsert(pkg->u.rx.root_of_sections, &section->leaf_of_sections, &section_compare_by_categroy);
        list_add_tail(&section->entry_of_sections, &pkg->u.rx.head_of_sections);
        ++pkg->u.rx.count_of_sections;

        /* move parse cursor */
        offset += sizeof(struct hdtp_section_head);
        offset += section->head->len;
        remain -= sizeof(struct hdtp_section_head);
        remain -= section->head->len;
    }

    if (!NSP_SUCCESS(status)) {
        while (NULL != (section = list_first_entry_or_null(&pkg->u.rx.head_of_sections, struct hdtp_section, entry_of_sections)) ) {
            list_del(&section->entry_of_sections);
            zfree(section);
        }

        zfree(pkg);
    } else {
        *package = pkg;
    }

    return status;
}

nsp_status_t query_package_base(const hdtp_package_pt package, hdtp_uint16_t *seq, hdtp_int32_t *type, hdtp_int32_t *error)
{
    if (!package) {
        return posix__makeerror(EINVAL);
    }

    if (0 == package->u.rx.size) {
        return posix__makeerror(EINVAL);
    }

    if (seq) {
        *seq = package->app_head->seq;
    }

    if (type) {
        *type = package->app_head->type;
    }

    if (error) {
        *error = package->app_head->err;
    }

    return NSP_STATUS_SUCCESSFUL;
}

hdtp_uint16_t hdtp_query_section(const hdtp_package_pt package, hdtp_uint16_t categroy, const void **sectdata)
{
    struct avltree_node_t *target;
    struct hdtp_section section, *tagsection;

    if (!package) {
        return 0;
    }

    section.categroy = categroy;
    target = avlsearch(package->u.rx.root_of_sections, &section.leaf_of_sections, &section_compare_by_categroy);
    if (!target) {
        return 0;
    }

    tagsection = container_of(target, struct hdtp_section, leaf_of_sections);
    if (sectdata) {
        *sectdata = tagsection->head->payload;
    }

    return tagsection->head->len;
}
