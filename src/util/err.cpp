/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "err.hpp"
#include "macros.hpp"

const char *slk::errno_to_string(int errno_)
{
    switch (errno_) {
#ifdef _WIN32
        case ENOTSUP:
            return "Not supported";
        case EPROTONOSUPPORT:
            return "Protocol not supported";
        case ENOBUFS:
            return "No buffer space available";
        case ENETDOWN:
            return "Network is down";
        case EADDRINUSE:
            return "Address in use";
        case EADDRNOTAVAIL:
            return "Address not available";
        case ECONNREFUSED:
            return "Connection refused";
        case EINPROGRESS:
            return "Operation in progress";
#endif
        case SL_EFSM:
            return "Operation cannot be accomplished in current state";
        case SL_ENOCOMPATPROTO:
            return "The protocol is not compatible with the socket type";
        case SL_ETERM:
            return "Context was terminated";
        case SL_EMTHREAD:
            return "No thread available";
        case EHOSTUNREACH:
            return "Host unreachable";
        default:
#if defined _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
            return strerror(errno_);
#if defined _MSC_VER
#pragma warning(pop)
#endif
    }
}

void slk::slk_abort(const char *errmsg_)
{
#ifdef _WIN32
    // Raise STATUS_FATAL_APP_EXIT.
    ULONG_PTR extra_info[1];
    extra_info[0] = (ULONG_PTR) errmsg_;
    RaiseException(0x40000015, EXCEPTION_NONCONTINUABLE, 1, extra_info);
#else
    SL_UNUSED(errmsg_);
    print_backtrace();
    abort();
#endif
}

#ifdef _WIN32

const char *slk::wsa_error()
{
    return wsa_error_no(WSAGetLastError(), nullptr);
}

const char *slk::wsa_error_no(int no_, const char *wsae_wouldblock_string_)
{
    switch (no_) {
        case WSABASEERR:
            return "No Error";
        case WSAEINTR:
            return "Interrupted system call";
        case WSAEBADF:
            return "Bad file number";
        case WSAEACCES:
            return "Permission denied";
        case WSAEFAULT:
            return "Bad address";
        case WSAEINVAL:
            return "Invalid argument";
        case WSAEMFILE:
            return "Too many open files";
        case WSAEWOULDBLOCK:
            return wsae_wouldblock_string_;
        case WSAEINPROGRESS:
            return "Operation now in progress";
        case WSAEALREADY:
            return "Operation already in progress";
        case WSAENOTSOCK:
            return "Socket operation on non-socket";
        case WSAEDESTADDRREQ:
            return "Destination address required";
        case WSAEMSGSIZE:
            return "Message too long";
        case WSAEPROTOTYPE:
            return "Protocol wrong type for socket";
        case WSAENOPROTOOPT:
            return "Bad protocol option";
        case WSAEPROTONOSUPPORT:
            return "Protocol not supported";
        case WSAESOCKTNOSUPPORT:
            return "Socket type not supported";
        case WSAEOPNOTSUPP:
            return "Operation not supported on socket";
        case WSAEPFNOSUPPORT:
            return "Protocol family not supported";
        case WSAEAFNOSUPPORT:
            return "Address family not supported by protocol family";
        case WSAEADDRINUSE:
            return "Address already in use";
        case WSAEADDRNOTAVAIL:
            return "Can't assign requested address";
        case WSAENETDOWN:
            return "Network is down";
        case WSAENETUNREACH:
            return "Network is unreachable";
        case WSAENETRESET:
            return "Net dropped connection or reset";
        case WSAECONNABORTED:
            return "Software caused connection abort";
        case WSAECONNRESET:
            return "Connection reset by peer";
        case WSAENOBUFS:
            return "No buffer space available";
        case WSAEISCONN:
            return "Socket is already connected";
        case WSAENOTCONN:
            return "Socket is not connected";
        case WSAESHUTDOWN:
            return "Can't send after socket shutdown";
        case WSAETOOMANYREFS:
            return "Too many references can't splice";
        case WSAETIMEDOUT:
            return "Connection timed out";
        case WSAECONNREFUSED:
            return "Connection refused";
        case WSAELOOP:
            return "Too many levels of symbolic links";
        case WSAENAMETOOLONG:
            return "File name too long";
        case WSAEHOSTDOWN:
            return "Host is down";
        case WSAEHOSTUNREACH:
            return "No Route to Host";
        case WSAENOTEMPTY:
            return "Directory not empty";
        case WSAEPROCLIM:
            return "Too many processes";
        case WSAEUSERS:
            return "Too many users";
        case WSAEDQUOT:
            return "Disc Quota Exceeded";
        case WSAESTALE:
            return "Stale NFS file handle";
        case WSAEREMOTE:
            return "Too many levels of remote in path";
        case WSASYSNOTREADY:
            return "Network SubSystem is unavailable";
        case WSAVERNOTSUPPORTED:
            return "WINSOCK DLL Version out of range";
        case WSANOTINITIALISED:
            return "Successful WSASTARTUP not yet performed";
        case WSAHOST_NOT_FOUND:
            return "Host not found";
        case WSATRY_AGAIN:
            return "Non-Authoritative Host not found";
        case WSANO_RECOVERY:
            return "Non-Recoverable errors: FORMERR REFUSED NOTIMP";
        case WSANO_DATA:
            return "Valid name no data record of requested";
        default:
            return "error not defined";
    }
}

void slk::win_error(char *buffer_, size_t buffer_size_)
{
    const DWORD errcode = GetLastError();
    const DWORD rc = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errcode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer_,
        static_cast<DWORD>(buffer_size_), nullptr);
    slk_assert(rc);
}

int slk::wsa_error_to_errno(int errcode_)
{
    switch (errcode_) {
        case WSAEINTR:
            return EINTR;
        case WSAEBADF:
            return EBADF;
        case WSAEACCES:
            return EACCES;
        case WSAEFAULT:
            return EFAULT;
        case WSAEINVAL:
            return EINVAL;
        case WSAEMFILE:
            return EMFILE;
        case WSAEWOULDBLOCK:
            return EBUSY;
        case WSAEINPROGRESS:
            return EAGAIN;
        case WSAEALREADY:
            return EAGAIN;
        case WSAENOTSOCK:
            return ENOTSOCK;
        case WSAEDESTADDRREQ:
            return EFAULT;
        case WSAEMSGSIZE:
            return EMSGSIZE;
        case WSAEPROTOTYPE:
            return EFAULT;
        case WSAENOPROTOOPT:
            return EINVAL;
        case WSAEPROTONOSUPPORT:
            return EPROTONOSUPPORT;
        case WSAESOCKTNOSUPPORT:
            return EFAULT;
        case WSAEOPNOTSUPP:
            return EFAULT;
        case WSAEPFNOSUPPORT:
            return EPROTONOSUPPORT;
        case WSAEAFNOSUPPORT:
            return EAFNOSUPPORT;
        case WSAEADDRINUSE:
            return EADDRINUSE;
        case WSAEADDRNOTAVAIL:
            return EADDRNOTAVAIL;
        case WSAENETDOWN:
            return ENETDOWN;
        case WSAENETUNREACH:
            return ENETUNREACH;
        case WSAENETRESET:
            return ENETRESET;
        case WSAECONNABORTED:
            return ECONNABORTED;
        case WSAECONNRESET:
            return ECONNRESET;
        case WSAENOBUFS:
            return ENOBUFS;
        case WSAEISCONN:
            return EFAULT;
        case WSAENOTCONN:
            return ENOTCONN;
        case WSAESHUTDOWN:
            return EFAULT;
        case WSAETOOMANYREFS:
            return EFAULT;
        case WSAETIMEDOUT:
            return ETIMEDOUT;
        case WSAECONNREFUSED:
            return ECONNREFUSED;
        case WSAELOOP:
            return EFAULT;
        case WSAENAMETOOLONG:
            return EFAULT;
        case WSAEHOSTDOWN:
            return EAGAIN;
        case WSAEHOSTUNREACH:
            return EHOSTUNREACH;
        case WSAENOTEMPTY:
            return EFAULT;
        case WSAEPROCLIM:
            return EFAULT;
        case WSAEUSERS:
            return EFAULT;
        case WSAEDQUOT:
            return EFAULT;
        case WSAESTALE:
            return EFAULT;
        case WSAEREMOTE:
            return EFAULT;
        case WSASYSNOTREADY:
            return EFAULT;
        case WSAVERNOTSUPPORTED:
            return EFAULT;
        case WSANOTINITIALISED:
            return EFAULT;
        case WSAHOST_NOT_FOUND:
            return EFAULT;
        case WSATRY_AGAIN:
            return EFAULT;
        case WSANO_RECOVERY:
            return EFAULT;
        case WSANO_DATA:
            return EFAULT;
        default:
            wsa_assert(false);
    }
    // Not reachable
    return 0;
}

#endif

void slk::print_backtrace()
{
    // Backtrace support can be added later if needed
}
