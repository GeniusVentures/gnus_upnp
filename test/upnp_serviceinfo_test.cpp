#include <upnp_igd_parser.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>
#include <sstream>

using namespace sgns::upnp;

namespace {

    /// Create a logger that captures output to a stringstream for verification.
    std::pair<std::shared_ptr<spdlog::logger>, std::shared_ptr<std::ostringstream>>
    makeCaptureLogger() {
        auto oss = std::make_shared<std::ostringstream>();
        auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(*oss);
        auto logger = std::make_shared<spdlog::logger>("test", sink);
        logger->set_level(spdlog::level::err);
        return {logger, oss};
    }

    boost::property_tree::ptree parseXML(const std::string &xml) {
        std::istringstream iss(xml);
        boost::property_tree::ptree tree;
        boost::property_tree::read_xml(iss, tree);
        return tree;
    }

    // ── Sample XML fixtures ──────────────────────────────────────────

    /// Minimal IGD v1 with WANIPConnection service.
    constexpr const char *kIGDv1XML = R"(<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
  <device>
    <deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType>
    <deviceList>
      <device>
        <deviceType>urn:schemas-upnp-org:device:WANDevice:1</deviceType>
        <deviceList>
          <device>
            <deviceType>urn:schemas-upnp-org:device:WANConnectionDevice:1</deviceType>
            <serviceList>
              <service>
                <serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>
                <controlURL>/ctl/IPConn</controlURL>
              </service>
            </serviceList>
          </device>
        </deviceList>
      </device>
    </deviceList>
  </device>
</root>
)";

    /// IGD v2 — same structure, different version numbers.
    constexpr const char *kIGDv2XML = R"(<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
  <device>
    <deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:2</deviceType>
    <deviceList>
      <device>
        <deviceType>urn:schemas-upnp-org:device:WANDevice:2</deviceType>
        <deviceList>
          <device>
            <deviceType>urn:schemas-upnp-org:device:WANConnectionDevice:2</deviceType>
            <serviceList>
              <service>
                <serviceType>urn:schemas-upnp-org:service:WANIPConnection:2</serviceType>
                <controlURL>/upnp/control/WANIPConn1</controlURL>
              </service>
            </serviceList>
          </device>
        </deviceList>
      </device>
    </deviceList>
  </device>
</root>
)";

    /// Philips Hue Bridge — real XML from user logs.
    constexpr const char *kPhilipsHueXML = R"(<?xml version="1.0" encoding="UTF-8" ?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
<specVersion>
<major>1</major>
<minor>0</minor>
</specVersion>
<URLBase>http://192.168.50.141:80/</URLBase>
<device>
<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>
<friendlyName>Hue Bridge (192.168.50.141)</friendlyName>
<manufacturer>Signify</manufacturer>
<modelDescription>Philips hue Personal Wireless Lighting</modelDescription>
<modelName>Philips hue bridge 2015</modelName>
<serialNumber>ecb5fa2cbf8d</serialNumber>
<UDN>uuid:2f402f80-da50-11e1-9b23-ecb5fa2cbf8d</UDN>
</device>
</root>
)";

    /// Empty device element.
    constexpr const char *kEmptyDeviceXML = R"(<?xml version="1.0"?>
<root><device/></root>
)";

    /// IGD without serviceList nesting (missing WANConnectionDevice level).
    constexpr const char *kIGDNoServiceListXML = R"(<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
  <device>
    <deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType>
  </device>
</root>
)";

    /// IGD missing serviceType child.
    constexpr const char *kIGDNoServiceTypeXML = R"(<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
  <device>
    <deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType>
    <deviceList>
      <device><deviceType>urn:schemas-upnp-org:device:WANDevice:1</deviceType>
        <deviceList>
          <device><deviceType>urn:schemas-upnp-org:device:WANConnectionDevice:1</deviceType>
            <serviceList>
              <service>
                <controlURL>/ctl/IPConn</controlURL>
              </service>
            </serviceList>
          </device>
        </deviceList>
      </device>
    </deviceList>
  </device>
</root>
)";

    /// IGD missing controlURL child.
    constexpr const char *kIGDNoControlURLXML = R"(<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
  <device>
    <deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType>
    <deviceList>
      <device><deviceType>urn:schemas-upnp-org:device:WANDevice:1</deviceType>
        <deviceList>
          <device><deviceType>urn:schemas-upnp-org:device:WANConnectionDevice:1</deviceType>
            <serviceList>
              <service>
                <serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>
              </service>
            </serviceList>
          </device>
        </deviceList>
      </device>
    </deviceList>
  </device>
</root>
)";

    /// XML with namespace prefixes — property_tree won't match the path.
    constexpr const char *kNamespacePrefixedXML = R"(<?xml version="1.0"?>
<ns0:root xmlns:ns0="urn:schemas-upnp-org:device-1-0">
  <ns0:device>
    <ns0:deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</ns0:deviceType>
  </ns0:device>
</ns0:root>
)";

}  // namespace

// ── Happy path ───────────────────────────────────────────────────────

TEST(ParseIGDServiceInfo, IGDv1) {
    auto [logger, oss] = makeCaptureLogger();
    auto tree = parseXML(kIGDv1XML);
    auto svc = parseIGDServiceInfo(tree, kIGDv1XML, logger);
    ASSERT_TRUE(svc.has_value());
    EXPECT_EQ(svc->serviceType,
              "urn:schemas-upnp-org:service:WANIPConnection:1");
    EXPECT_EQ(svc->controlURL, "/ctl/IPConn");
}

TEST(ParseIGDServiceInfo, IGDv2) {
    auto [logger, oss] = makeCaptureLogger();
    auto tree = parseXML(kIGDv2XML);
    auto svc = parseIGDServiceInfo(tree, kIGDv2XML, logger);
    ASSERT_TRUE(svc.has_value());
    EXPECT_EQ(svc->serviceType,
              "urn:schemas-upnp-org:service:WANIPConnection:2");
    EXPECT_EQ(svc->controlURL, "/upnp/control/WANIPConn1");
}

// ── Non-IGD devices ──────────────────────────────────────────────────

TEST(ParseIGDServiceInfo, PhilipsHue) {
    auto [logger, oss] = makeCaptureLogger();
    auto tree = parseXML(kPhilipsHueXML);
    auto svc = parseIGDServiceInfo(tree, kPhilipsHueXML, logger);
    EXPECT_FALSE(svc.has_value());
    // Verify raw XML was logged
    EXPECT_NE(oss->str().find("Raw XML"), std::string::npos);
    EXPECT_NE(oss->str().find("Philips hue"), std::string::npos);
}

TEST(ParseIGDServiceInfo, EmptyDevice) {
    auto [logger, oss] = makeCaptureLogger();
    auto tree = parseXML(kEmptyDeviceXML);
    auto svc = parseIGDServiceInfo(tree, kEmptyDeviceXML, logger);
    EXPECT_FALSE(svc.has_value());
}

TEST(ParseIGDServiceInfo, MissingServiceList) {
    auto [logger, oss] = makeCaptureLogger();
    auto tree = parseXML(kIGDNoServiceListXML);
    auto svc = parseIGDServiceInfo(tree, kIGDNoServiceListXML, logger);
    EXPECT_FALSE(svc.has_value());
}

TEST(ParseIGDServiceInfo, MalformedXML) {
    auto [logger, oss] = makeCaptureLogger();
    boost::property_tree::ptree tree;
    try {
        std::istringstream iss("not xml at all");
        boost::property_tree::read_xml(iss, tree);
    } catch (...) {
        // read_xml itself will throw on malformed input before we even
        // get to parseIGDServiceInfo.  This is expected behaviour.
        SUCCEED();
        return;
    }
    FAIL() << "read_xml should have thrown on malformed input";
}

TEST(ParseIGDServiceInfo, NamespacePrefixedXML) {
    auto [logger, oss] = makeCaptureLogger();
    auto tree = parseXML(kNamespacePrefixedXML);
    auto svc = parseIGDServiceInfo(tree, kNamespacePrefixedXML, logger);
    // Namespace prefixes break the dot-path matching
    EXPECT_FALSE(svc.has_value());
}

TEST(ParseIGDServiceInfo, MissingServiceType) {
    auto [logger, oss] = makeCaptureLogger();
    auto tree = parseXML(kIGDNoServiceTypeXML);
    auto svc = parseIGDServiceInfo(tree, kIGDNoServiceTypeXML, logger);
    EXPECT_FALSE(svc.has_value());
}

TEST(ParseIGDServiceInfo, MissingControlURL) {
    auto [logger, oss] = makeCaptureLogger();
    auto tree = parseXML(kIGDNoControlURLXML);
    auto svc = parseIGDServiceInfo(tree, kIGDNoControlURLXML, logger);
    EXPECT_FALSE(svc.has_value());
}

// ── Edge case: minimum valid structure ───────────────────────────────

TEST(ParseIGDServiceInfo, MinimumValidIGD) {
    auto [logger, oss] = makeCaptureLogger();
    auto tree = parseXML(kIGDv1XML);
    auto svc = parseIGDServiceInfo(tree, kIGDv1XML, logger);
    ASSERT_TRUE(svc.has_value());
    EXPECT_FALSE(svc->serviceType.empty());
    EXPECT_FALSE(svc->controlURL.empty());
}

// ── Non-IGD before IGD (discovery order) ─────────────────────────────

TEST(ParseIGDServiceInfo, NonIGDBeforeIGD) {
    auto [logger, oss] = makeCaptureLogger();

    // First: Philips Hue — should fail
    auto hueTree = parseXML(kPhilipsHueXML);
    auto hueSvc = parseIGDServiceInfo(hueTree, kPhilipsHueXML, logger);
    EXPECT_FALSE(hueSvc.has_value());

    // Second: real IGD — should succeed
    auto igdTree = parseXML(kIGDv1XML);
    auto igdSvc = parseIGDServiceInfo(igdTree, kIGDv1XML, logger);
    EXPECT_TRUE(igdSvc.has_value());
}
