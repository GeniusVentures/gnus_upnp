/**
 * Internal IGD root-description XML parser — extracted from UPNP class for
 * testability.  Not part of the public API.
 */
#ifndef GNUS_UPNP_IGD_PARSER_HPP
#define GNUS_UPNP_IGD_PARSER_HPP

#include <boost/optional.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <memory>
#include <string>

#include <spdlog/spdlog.h>

namespace sgns::upnp {

    struct IGDServiceInfo {
        std::string serviceType;
        std::string controlURL;
    };

    /**
     * Parse service info from the IGD root description XML.
     * Returns boost::none if the expected XML path is not found
     * (router uses a different UPnP device hierarchy).
     * On failure, logs the missing path AND the raw XML for diagnostics.
     */
    boost::optional<IGDServiceInfo> parseIGDServiceInfo(
        const boost::property_tree::ptree &tree,
        const std::string &rawXml,
        std::shared_ptr<spdlog::logger> logger);

}  // namespace sgns::upnp

#endif  // GNUS_UPNP_IGD_PARSER_HPP
