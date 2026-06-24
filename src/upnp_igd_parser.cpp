#include <upnp_igd_parser.hpp>

#include <boost/property_tree/ptree.hpp>

namespace sgns::upnp {

    boost::optional<IGDServiceInfo> parseIGDServiceInfo(
        const boost::property_tree::ptree &tree,
        const std::string &rawXml,
        std::shared_ptr<spdlog::logger> logger) {
        try {
            const std::string path =
                "root.device.deviceList.device.deviceList.device.serviceList.service";
            IGDServiceInfo info;
            info.serviceType = tree.get<std::string>(path + ".serviceType");
            info.controlURL  = tree.get<std::string>(path + ".controlURL");
            return info;
        } catch (const boost::property_tree::ptree_bad_path &e) {
            logger->error(
                "UPnP root description missing expected path: {}\n"
                "Raw XML (first 2000 chars): {}",
                e.what(),
                rawXml.substr(0, 2000));
            return boost::none;
        } catch (const std::exception &e) {
            logger->error(
                "Exception parsing UPnP root description: {}\n"
                "Raw XML (first 2000 chars): {}",
                e.what(),
                rawXml.substr(0, 2000));
            return boost::none;
        }
    }

}  // namespace sgns::upnp
