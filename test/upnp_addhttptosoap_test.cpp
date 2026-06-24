#include <upnp_util.hpp>

#include <gtest/gtest.h>

using namespace sgns::upnp;

TEST(AddHTTPtoSoap, StandardWrapping) {
    auto result = AddHTTPtoSoap("<s:Body>test</s:Body>",
                                "/control",
                                "192.168.1.1",
                                80,
                                "urn:schemas-upnp-org:service:WANIPConnection:1",
                                "#AddPortMapping");

    // Check HTTP method and path
    EXPECT_NE(result.find("POST /control HTTP/1.1\r\n"), std::string::npos);

    // Check HOST header
    EXPECT_NE(result.find("HOST: 192.168.1.1:80\r\n"), std::string::npos);

    // Check Content-Type
    EXPECT_NE(result.find("CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n"),
              std::string::npos);

    // Check SOAPACTION format
    EXPECT_NE(result.find("SOAPACTION: \"urn:schemas-upnp-org:service:WANIPConnection:1#AddPortMapping\"\r\n"),
              std::string::npos);

    // Check body present after header separator
    auto sep = result.find("\r\n\r\n");
    ASSERT_NE(sep, std::string::npos);
    EXPECT_EQ(result.substr(sep + 4), "<s:Body>test</s:Body>");
}

TEST(AddHTTPtoSoap, EmptySOAPBody) {
    auto result = AddHTTPtoSoap("", "/path", "10.0.0.1", 5000,
                                "device", "#action");

    // Content-Length should be 0
    EXPECT_NE(result.find("CONTENT-LENGTH: 0\r\n"), std::string::npos);

    // Body after \r\n\r\n should be empty
    auto sep = result.find("\r\n\r\n");
    ASSERT_NE(sep, std::string::npos);
    EXPECT_EQ(result.substr(sep + 4), "");
}

TEST(AddHTTPtoSoap, SOAPACTIONFormat) {
    auto result = AddHTTPtoSoap("<x/>", "/", "host", 1234,
                                "urn:test", "#DoSomething");

    EXPECT_NE(result.find("SOAPACTION: \"urn:test#DoSomething\"\r\n"),
              std::string::npos);
}

TEST(AddHTTPtoSoap, ContentLengthMatchesBodySize) {
    std::string body = "<s:Envelope><s:Body>hello world</s:Body></s:Envelope>";
    auto result = AddHTTPtoSoap(body, "/", "h", 1, "d", "#a");

    std::string expectedLen =
        "CONTENT-LENGTH: " + std::to_string(body.size());
    EXPECT_NE(result.find(expectedLen), std::string::npos);
}
