/// @file  upnp_rootdesc_test.cpp
/// Tests for the device-type verification logic in GetRootDesc().
///
/// Because GetRootDesc() is a member function that performs network I/O,
/// we test the extracted device-type check logic directly via
/// boost::property_tree.

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace {

    /// Replicates the device-type check from GetRootDesc().
    /// Returns true if the XML describes an InternetGatewayDevice.
    bool isIGDDeviceType(const std::string &xml) {
        try {
            std::istringstream iss(xml);
            boost::property_tree::ptree tree;
            boost::property_tree::read_xml(iss, tree);
            auto deviceType = tree.get<std::string>("root.device.deviceType");
            return deviceType.find("InternetGatewayDevice") != std::string::npos;
        } catch (const boost::property_tree::ptree_bad_path &) {
            return false;
        } catch (const std::exception &) {
            return false;
        }
    }

    // ── Sample XML fixtures ──────────────────────────────────────────

    constexpr const char *kIGDv1XML = R"(<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
  <device>
    <deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType>
  </device>
</root>
)";

    constexpr const char *kIGDv2XML = R"(<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
  <device>
    <deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:2</deviceType>
  </device>
</root>
)";

    constexpr const char *kPhilipsHueXML = R"(<?xml version="1.0" encoding="UTF-8" ?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
<device>
<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>
<friendlyName>Hue Bridge</friendlyName>
<manufacturer>Signify</manufacturer>
</device>
</root>
)";

    constexpr const char *kNoDeviceTypeXML = R"(<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
  <device>
    <friendlyName>NoDeviceType</friendlyName>
  </device>
</root>
)";

    constexpr const char *kMediaRendererXML = R"(<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
  <device>
    <deviceType>urn:schemas-upnp-org:device:MediaRenderer:1</deviceType>
    <friendlyName>Living Room TV</friendlyName>
  </device>
</root>
)";

}  // namespace

TEST(GetRootDescDeviceType, IGDv1Accepted) {
    EXPECT_TRUE(isIGDDeviceType(kIGDv1XML));
}

TEST(GetRootDescDeviceType, IGDv2Accepted) {
    EXPECT_TRUE(isIGDDeviceType(kIGDv2XML));
}

TEST(GetRootDescDeviceType, NonIGDRejected_PhilipsHue) {
    EXPECT_FALSE(isIGDDeviceType(kPhilipsHueXML));
}

TEST(GetRootDescDeviceType, MissingDeviceTypeRejected) {
    EXPECT_FALSE(isIGDDeviceType(kNoDeviceTypeXML));
}

TEST(GetRootDescDeviceType, MalformedXMLRejected) {
    EXPECT_FALSE(isIGDDeviceType("not valid xml <<<"));
}
