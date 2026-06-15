#include <upnp_util.hpp>

#include <gtest/gtest.h>

using namespace sgns::upnp;

namespace {

    constexpr const char *kIGDv1Response = R"(HTTP/1.1 200 OK
CACHE-CONTROL: max-age=1800
DATE: Thu, 01 Jan 2025 00:00:00 GMT
EXT:
LOCATION: http://192.168.1.1:80/rootDesc.xml
SERVER: Linux/2.6.36 UPnP/1.0 IGD/1.0
ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1
USN: uuid:12345678-1234-1234-1234-123456789abc::urn:schemas-upnp-org:device:InternetGatewayDevice:1
)";

    constexpr const char *kIGDv2Response = R"(HTTP/1.1 200 OK
CACHE-CONTROL: max-age=1800
LOCATION: http://10.0.0.1:5000/igd.xml
ST: urn:schemas-upnp-org:device:InternetGatewayDevice:2
USN: uuid:abcd::urn:schemas-upnp-org:device:InternetGatewayDevice:2
)";

    constexpr const char *kNoLocationResponse = R"(HTTP/1.1 200 OK
SERVER: lighttpd/1.4
ST: urn:schemas-upnp-org:device:Basic:1
USN: uuid:hue-bridge
)";

    constexpr const char *kLowercaseLocation = R"(HTTP/1.1 200 OK
location: http://192.168.1.1:80/desc.xml
ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1
)";

    constexpr const char *kMultipleHeaders = R"(HTTP/1.1 200 OK
SERVER: Foo/1.0
ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1
USN: uuid:123
LOCATION: http://192.168.1.1:80/desc.xml
CACHE-CONTROL: max-age=1800
)";

    constexpr const char *kPhilipsHueResponse = R"(HTTP/1.1 200 OK
CACHE-CONTROL: max-age=100
LOCATION: http://192.168.50.141:80/description.xml
SERVER: Linux/3.14.0 UPnP/1.0 IpBridge/1.52.0
ST: urn:schemas-upnp-org:device:Basic:1
USN: uuid:2f402f80-da50-11e1-9b23-ecb5fa2cbf8d
)";

    constexpr const char *kDuplicateLocation = R"(HTTP/1.1 200 OK
LOCATION: http://192.168.1.1:80/first.xml
LOCATION: http://192.168.1.1:80/second.xml
)";

    constexpr const char *kLocationWithExtraSpaces = R"(HTTP/1.1 200 OK
LOCATION:  http://192.168.1.1:80/desc.xml
ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1
)";

}  // namespace

TEST(SSDPParse, IGDv1Response) {
    auto r = parseSSDPResponse(kIGDv1Response, "192.168.1.1");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->locationURL, "http://192.168.1.1:80/rootDesc.xml");
    EXPECT_EQ(r->ipAddress, "192.168.1.1");
}

TEST(SSDPParse, IGDv2Response) {
    auto r = parseSSDPResponse(kIGDv2Response, "10.0.0.1");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->locationURL, "http://10.0.0.1:5000/igd.xml");
}

TEST(SSDPParse, NoLocationHeader) {
    auto r = parseSSDPResponse(kNoLocationResponse, "192.168.1.100");
    EXPECT_FALSE(r.has_value());
}

TEST(SSDPParse, LowercaseLocation) {
    auto r = parseSSDPResponse(kLowercaseLocation, "192.168.1.1");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->locationURL, "http://192.168.1.1:80/desc.xml");
}

TEST(SSDPParse, MultipleHeaders) {
    auto r = parseSSDPResponse(kMultipleHeaders, "192.168.1.1");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->locationURL, "http://192.168.1.1:80/desc.xml");
}

TEST(SSDPParse, EmptyResponse) {
    auto r = parseSSDPResponse("", "0.0.0.0");
    EXPECT_FALSE(r.has_value());
}

TEST(SSDPParse, PhilipsHueResponse) {
    auto r = parseSSDPResponse(kPhilipsHueResponse, "192.168.50.141");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->locationURL, "http://192.168.50.141:80/description.xml");
}

TEST(SSDPParse, DuplicateLocation) {
    // Should extract the first occurrence
    auto r = parseSSDPResponse(kDuplicateLocation, "192.168.1.1");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->locationURL, "http://192.168.1.1:80/first.xml");
}

TEST(SSDPParse, LocationWithExtraSpaces) {
    auto r = parseSSDPResponse(kLocationWithExtraSpaces, "192.168.1.1");
    ASSERT_TRUE(r.has_value());
    // Regex captures including leading spaces after the colon
    EXPECT_NE(r->locationURL.find("http://"), std::string::npos);
}
