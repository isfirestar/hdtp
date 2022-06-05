#include "hdtp.h"

#include "compiler.h"
#include "nis.h"

#include "section.h"

hdtp_status_t hdtp_write(HDTPLINK link, const hdtp_package_pt package)
{
    if (!package) {
        return posix__makeerror(EINVAL);
    }

    return tcp_write(link, package->u.tx.raw, HDTP_BASE_HEAD_LEN + package->low_head->len, NULL);
}
