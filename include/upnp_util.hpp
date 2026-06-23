/**
 * Internal utility functions for UPnP — extracted from UPNP class for
 * testability.  Not part of the public API.
 */
#ifndef GNUS_UPNP_UTIL_HPP
#define GNUS_UPNP_UTIL_HPP

#include <boost/optional.hpp>
#include <string>

namespace sgns::upnp {

    /// Result of parsing an SSDP M-SEARCH response.
    struct SSDPResponse {
        std::string locationURL;  // rootDesc XML URL
        std::string ipAddress;    // responding device IP
    };

    /**
     * Parse a URL into host, port, and path components.
     * Only supports http:// URLs.
     * @returns true on success
     */
    bool ParseURL(const std::string &url,
                  std::string &host,
                  unsigned short &port,
                  std::string &path);

    /**
     * Add HTTP POST headers to a SOAP XML body.
     * @param soapxml  The SOAP XML payload
     * @param path     URL path from the IGD controlURL
     * @param host     IGD host
     * @param port     IGD port
     * @param device   Device URN (e.g. the serviceType)
     * @param action   SOAP action (e.g. "#AddPortMapping")
     * @returns Full HTTP request with headers and body
     */
    std::string AddHTTPtoSoap(std::string soapxml,
                              std::string path,
                              std::string host,
                              unsigned short port,
                              std::string device,
                              std::string action);

    /**
     * Parse an SSDP M-SEARCH response for the LOCATION header.
     * @param response  Raw SSDP response text
     * @param ipAddress IP of the responding device
     * @returns boost::none if no LOCATION header found
     */
    boost::optional<SSDPResponse> parseSSDPResponse(
        const std::string &response,
        const std::string &ipAddress);

}  // namespace sgns::upnp

#endif  // GNUS_UPNP_UTIL_HPP
