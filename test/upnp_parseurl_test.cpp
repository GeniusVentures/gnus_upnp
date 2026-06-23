#include <upnp_util.hpp>

#include <gtest/gtest.h>

using namespace sgns::upnp;

namespace {
    struct ParseURLResult {
        bool valid;
        std::string host;
        unsigned short port;
        std::string path;
    };

    ParseURLResult parse(const std::string &url) {
        ParseURLResult r{};
        std::string h, p;
        r.valid = ParseURL(url, h, r.port, p);
        r.host = h;
        r.path = p;
        return r;
    }
}  // namespace

TEST(ParseURL, StandardHTTPURL) {
    auto r = parse("http://192.168.1.1:80/desc.xml");
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.host, "192.168.1.1");
    EXPECT_EQ(r.port, 80);
    EXPECT_EQ(r.path, "/desc.xml");
}

TEST(ParseURL, URLWithoutPath) {
    auto r = parse("http://10.0.0.1:5000");
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.host, "10.0.0.1");
    EXPECT_EQ(r.port, 5000);
    EXPECT_EQ(r.path, "");
}

TEST(ParseURL, URLWithDeepPath) {
    auto r = parse("http://1.2.3.4:8080/a/b/c.xml");
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.host, "1.2.3.4");
    EXPECT_EQ(r.port, 8080);
    EXPECT_EQ(r.path, "/a/b/c.xml");
}

TEST(ParseURL, MissingPort) {
    auto r = parse("http://192.168.1.1/desc.xml");
    EXPECT_FALSE(r.valid);
}

TEST(ParseURL, NoHTTPScheme) {
    auto r = parse("192.168.1.1:80/desc.xml");
    EXPECT_FALSE(r.valid);
}

TEST(ParseURL, EmptyString) {
    auto r = parse("");
    EXPECT_FALSE(r.valid);
}

TEST(ParseURL, HTTPSUnsupported) {
    auto r = parse("https://1.2.3.4:443/x.xml");
    EXPECT_FALSE(r.valid);
}

TEST(ParseURL, MaxPortNumber) {
    auto r = parse("http://1.2.3.4:65535/x");
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.port, 65535);
}
