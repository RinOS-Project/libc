/* RinOS libc syslog process-owned configuration. */

#include "syslog.h"
#include "pthread.h"
#include <rin/syslog_transport_v1.h>

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static pthread_mutex_t g_syslog_lock = PTHREAD_MUTEX_INITIALIZER;
static char g_syslog_ident[RIN_SYSLOG_IDENT_MAX + 1u];
static uint64_t g_syslog_request_id;

const char* _syslog_ident = NULL;
int _syslog_option = 0;
int _syslog_facility = LOG_USER;
int _syslog_mask = 0xFF;

void _rin_syslog_lock(void) {
    (void)pthread_mutex_lock(&g_syslog_lock);
}

void _rin_syslog_unlock(void) {
    (void)pthread_mutex_unlock(&g_syslog_lock);
}

/* Called under g_syslog_lock. Copy the identifier so subsequent logging does
 * not depend on caller-owned storage remaining alive after openlog(). */
void _rin_syslog_set_ident_locked(const char* ident) {
    if (!ident) {
        g_syslog_ident[0] = '\0';
        _syslog_ident = NULL;
        return;
    }

    size_t length = 0u;
    while (length < RIN_SYSLOG_IDENT_MAX && ident[length] != '\0') {
        g_syslog_ident[length] = ident[length];
        ++length;
    }
    g_syslog_ident[length] = '\0';
    _syslog_ident = g_syslog_ident;
}

static int syslog_wait_socket(int fd, short events)
{
    struct pollfd descriptor;
    int result;
    int interrupted = 0;
    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.fd = fd;
    descriptor.events = events;
    do {
        result = poll(&descriptor, 1u, 50);
        if (result < 0 && errno == EINTR && ++interrupted < 4) continue;
        break;
    } while (1);
    return result == 1 && (descriptor.revents & events) != 0 &&
           (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) == 0;
}

static int syslog_send_exact(int fd, const void* data, size_t size)
{
    const uint8_t* bytes = (const uint8_t*)data;
    size_t offset = 0u;
    unsigned int retries = 0u;
    while (offset < size && retries < 8u) {
        const ssize_t count = send(fd, bytes + offset, size - offset,
                                   MSG_DONTWAIT | MSG_NOSIGNAL);
        if (count < 0 && errno == EINTR) {
            ++retries;
            continue;
        }
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            ++retries;
            if (!syslog_wait_socket(fd, POLLOUT)) return 0;
            continue;
        }
        if (count <= 0 || (size_t)count > size - offset) return 0;
        offset += (size_t)count;
        retries = 0u;
    }
    return offset == size;
}

static int syslog_receive_exact(int fd, void* data, size_t size)
{
    uint8_t* bytes = (uint8_t*)data;
    size_t offset = 0u;
    unsigned int retries = 0u;
    while (offset < size && retries < 8u) {
        const ssize_t count = recv(fd, bytes + offset, size - offset,
                                   MSG_DONTWAIT);
        if (count < 0 && errno == EINTR) {
            ++retries;
            continue;
        }
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            ++retries;
            if (!syslog_wait_socket(fd, POLLIN)) return 0;
            continue;
        }
        if (count <= 0 || (size_t)count > size - offset) return 0;
        offset += (size_t)count;
        retries = 0u;
    }
    return offset == size;
}

static int syslog_transport_reply_ok(const RinSyslogTransportReplyV1* reply,
                                    uint64_t request_id)
{
    return reply->struct_size == sizeof(*reply) &&
           reply->version == RIN_SYSLOG_TRANSPORT_VERSION &&
           reply->reserved0 == 0u && reply->request_id == request_id &&
           reply->status == RIN_SYSLOG_TRANSPORT_STATUS_OK &&
           reply->reserved1 == 0u && reply->reserved[0] == 0u &&
           reply->reserved[1] == 0u;
}

int _rin_syslog_transport_send(unsigned int facility,
                               unsigned int severity,
                               const char* ident,
                               const char* message, size_t length)
{
    RinServiceLogServiceRequestV1 envelope;
    RinSyslogTransportRequestV1 request;
    RinSyslogTransportReplyV1 reply;
    struct sockaddr_un address;
    uint64_t request_id;
    size_t ident_length = 0u;
    int fd = -1;
    int socket_error = 0;
    socklen_t socket_error_size = sizeof(socket_error);
    int saved_errno = errno;
    int success = 0;
    if (facility >= 24u || severity >= 8u || ident == NULL || message == NULL ||
        length == 0u || length >= RIN_SYSLOG_TRANSPORT_MESSAGE_MAX)
        goto done;

    _rin_syslog_lock();
    if (g_syslog_request_id >= UINT64_MAX - 1u) g_syslog_request_id = 0u;
    request_id = ++g_syslog_request_id;
    _rin_syslog_unlock();

    memset(&envelope, 0, sizeof(envelope));
    envelope.struct_size = sizeof(envelope);
    envelope.version = RIN_SERVICE_LOG_SERVICE_VERSION;
    envelope.mode = RIN_SYSLOG_TRANSPORT_MODE_SUBMIT;
    envelope.limit = 1u;
    envelope.request_id = request_id;
    memcpy(envelope.service_id, RIN_SYSLOG_TRANSPORT_SERVICE_ID,
           sizeof(RIN_SYSLOG_TRANSPORT_SERVICE_ID));

    memset(&request, 0, sizeof(request));
    request.struct_size = sizeof(request);
    request.version = RIN_SYSLOG_TRANSPORT_VERSION;
    request.request_id = request_id;
    request.facility = facility;
    request.severity = severity;
    request.message_length = (uint32_t)length;
    while (ident_length + 1u < sizeof(request.ident) &&
           ident[ident_length] != '\0') {
        const unsigned char character = (unsigned char)ident[ident_length];
        request.ident[ident_length] =
            character >= 0x20u && character <= 0x7eu ? (char)character : '?';
        ++ident_length;
    }
    memcpy(request.message, message, length);
    request.message[length] = '\0';

    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    if (strlen(RIN_SERVICE_LOG_SERVICE_PATH) >= sizeof(address.sun_path))
        goto done;
    memcpy(address.sun_path, RIN_SERVICE_LOG_SERVICE_PATH,
           strlen(RIN_SERVICE_LOG_SERVICE_PATH) + 1u);
    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) goto done;
    if (connect(fd, (const struct sockaddr*)&address, sizeof(address)) != 0) {
        if ((errno != EINPROGRESS && errno != EAGAIN && errno != EWOULDBLOCK) ||
            !syslog_wait_socket(fd, POLLOUT) ||
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error,
                       &socket_error_size) != 0 || socket_error != 0)
            goto done;
    }
    if (!syslog_send_exact(fd, &envelope, sizeof(envelope)) ||
        !syslog_send_exact(fd, &request, sizeof(request)) ||
        !syslog_receive_exact(fd, &reply, sizeof(reply)) ||
        !syslog_transport_reply_ok(&reply, request_id)) goto done;
    success = 1;

done:
    if (fd >= 0) close(fd);
    errno = saved_errno;
    return success ? 0 : -1;
}
