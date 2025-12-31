/* SPDX-License-Identifier: MPL-2.0 */
/* ServerLink - Ported from libzmq */

#include "endpoint.hpp"

namespace slk
{

endpoint_uri_pair_t
make_unconnected_connect_endpoint_pair (const std::string &endpoint_)
{
    return endpoint_uri_pair_t (std::string (), endpoint_,
                                endpoint_type_connect);
}

endpoint_uri_pair_t
make_unconnected_bind_endpoint_pair (const std::string &endpoint_)
{
    return endpoint_uri_pair_t (endpoint_, std::string (), endpoint_type_bind);
}

}
