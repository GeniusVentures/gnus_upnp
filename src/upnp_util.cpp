#include <upnp_util.hpp>

#include <regex>
#include <stdexcept>

namespace sgns::upnp {

    bool ParseURL(const std::string &url,
                  std::string &host,
                  unsigned short &port,
                  std::string &path) {
        std::regex urlRegex(R"(http://([^:/]+):(\d+)(.*))");
        std::smatch match;
        if (std::regex_match(url, match, urlRegex)) {
            host = match[1];
            port = static_cast<unsigned short>(std::stoi(match[2]));
            path = match[3];
            return true;
        }
        return false;
    }

    std::string AddHTTPtoSoap(std::string soapxml,
                              std::string path,
                              std::string host,
                              unsigned short port,
                              std::string device,
                              std::string action) {
        std::string httpRequest = "POST " + path + " HTTP/1.1\r\n";
        httpRequest += "HOST: " + host + ":" + std::to_string(port) + "\r\n";
        httpRequest += "CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n";
        httpRequest += "CONTENT-LENGTH: " + std::to_string(soapxml.size()) +
                       "\r\n";
        httpRequest += "SOAPACTION: \"" + device + action + "\"\r\n";
        httpRequest += "\r\n";  // End of headers
        httpRequest += soapxml;
        return httpRequest;
    }

    boost::optional<SSDPResponse> parseSSDPResponse(
        const std::string &response,
        const std::string &ipAddress) {
        std::regex locationRegex(R"(LOCATION:\s*(.*))",
                                 std::regex_constants::icase);
        std::smatch match;
        if (std::regex_search(response, match, locationRegex)) {
            SSDPResponse info;
            info.ipAddress   = ipAddress;
            info.locationURL = match[1].str();
            return info;
        }
        return boost::none;
    }

}  // namespace sgns::upnp
