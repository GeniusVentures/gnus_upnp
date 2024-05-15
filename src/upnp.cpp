#include <upnp.hpp>
#include <iostream>
#include <string>
#include <regex>

namespace sgns::upnp
{
    bool UPNP::GetIGD()
    {
        std::chrono::seconds timeout(2);
        std::array<const char*, 2> search_targets
            = { "urn:schemas-upnp-org:device:InternetGatewayDevice:1"
              , "urn:schemas-upnp-org:device:InternetGatewayDevice:2"
            //, "urn:schemas-upnp-org:service:WANIPConnection:1"
            //, "urn:schemas-upnp-org:service:WANIPConnection:2"
            //, "urn:schemas-upnp-org:service:WANPPPConnection:1"
            //, "urn:schemas-upnp-org:service:WANPPPConnection:2"
            //, "upnp:rootdevice"
        };
        for (auto target : search_targets) {
            std::stringstream ss;
            ss << "M-SEARCH * HTTP/1.1\r\n"
                << "HOST: " << _multicast << "\r\n"
                << "ST: " << target << "\r\n"
                << "MAN: \"ssdp:discover\"\r\n"
                << "MX: " << timeout.count() << "\r\n"
                << "USER-AGENT: asio-upnp/1.0\r\n"
                << "\r\n";
            auto sss = ss.str();
            _socket.async_send_to(boost::asio::buffer(sss.data(), sss.size()), _multicast, [&](const boost::system::error_code& error, size_t bytes)
                {
                    std::cout << "Sent to" << std::endl;
                    if (!error)
                    {
                        std::array<char, 32 * 1024> rx;
                        boost::asio::mutable_buffer receive_buffer(rx.data(), rx.size());

                        boost::asio::ip::udp::endpoint remote_endpoint;

                        // Operation succeeded
                        std::cout << "Sent " << bytes << " bytes to the multicast endpoint." << std::endl;
                        // Now wait for a response
                        _socket.async_receive_from(receive_buffer, remote_endpoint,
                            [&](const boost::system::error_code& receive_error, size_t bytes_received) {
                                if (!receive_error) {
                                    std::cout << "Received " << bytes_received << " bytes from " << remote_endpoint << std::endl;
                                    //auto buffer = std::make_shared<std::vector<char>>(boost::asio::buffers_begin(receive_buffer->data()), boost::asio::buffers_end(receive_buffer->data()));
                                    ////Copy to a string
                                    //std::string bufferStr(buffer->begin(), buffer->end());
                                    //std::cout << "Received data: " << bufferStr << std::endl;
                                    std::string received_data(rx.data(), bytes_received);
                                    auto xmlavail = ParseIGD(received_data);

                                    //if (xmlavail && !_requestingRootDesc)
                                    //    GetRootDesc();
                                    //return true;
                                    //std::cout << "Received data: " << received_data << std::endl;
                                }
                                else {
                                    std::cerr << "Error receiving data: " << receive_error.message() << std::endl;
                                }
                            });
                    }
                    else
                    {
                        // Operation failed
                        std::cerr << "Error sending data: " << error.message() << std::endl;
                    }
                });
        }
        _ioc.run();
        _ioc.stop();
        _ioc.reset();
        _socket.close();
        for (const auto& rootDescXML : _rootDescXML) {
            auto gotrootdesc = GetRootDesc(rootDescXML);
            if (gotrootdesc) {
                std::cout << "Got root desc" << std::endl;
                return true;
            }
        }
        return false;
    }

    bool UPNP::ParseIGD(std::string& lines)
    {
        // Define a regex pattern to match the LOCATION header
        std::regex locationRegex(R"(LOCATION:\s*(.*))", std::regex_constants::icase);

        // Search for the LOCATION header in the response
        std::smatch match;
        if (std::regex_search(lines, match, locationRegex)) {
            // Extract and return the location URL
            _rootDescXML.push_back(match[1].str());
            std::cout << "String Location:" << match[1].str() << std::endl;
            return true;
        }
        else {
            // No LOCATION header found
            std::cout << "No header found" << std::endl;
            return false;
        }
    }

    bool UPNP::GetRootDesc(std::string xml)
    {
        //Parse URL to get correct elements for HTTP get and entpoint.
        std::string host;
        unsigned short port;
        std::string path;
        if (!ParseURL(xml, host, port, path))
        {
            return false;
        }
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(host), port);

        //Connect 
        //TODO: Support HTTPS because some routers use this.
        bool gotparse = false;
        _ioc.reset();
        std::string get_request = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
        _tcpsocket.async_connect(endpoint, [&](const boost::system::error_code& connect_error)
            {
                if (!connect_error)
                {

                    auto write_buffer = boost::asio::buffer(get_request);
                    std::cout << "1" << get_request << std::endl;

                    boost::asio::async_write(_tcpsocket, write_buffer, [&](const boost::system::error_code& write_error, std::size_t)
                        {
                            std::cout << "2" << std::endl;
                            //boost::asio::streambuf rootdesc;
                            auto rootdesc = std::make_shared<boost::asio::streambuf>();
                            boost::asio::async_read(_tcpsocket, *rootdesc, boost::asio::transfer_all(), [&, rootdesc](const boost::system::error_code& read_error, std::size_t bytes_transferred)
                                {
                                    std::cout << "3" << std::endl;
                                    auto buffer = std::vector<char>(boost::asio::buffers_begin(rootdesc->data()), boost::asio::buffers_end(rootdesc->data()));
                                    std::string bufferStr(buffer.begin(), buffer.end());
                                    auto bodyStartPos = bufferStr.find("\r\n\r\n");
                                    if (bodyStartPos != std::string::npos) {

                                        bufferStr = bufferStr.substr(bodyStartPos + 4);
                                        _rootDescData = bufferStr;
                                        //ParseRootDesc(bufferStr);
                                        gotparse = true;
                                    }
                                    else {
                                        gotparse = false;
                                        std::cerr << "Error Reading" << std::endl;
                                    }
                                });
                        });
                }
                else {
                    std::cerr << "Connection error: " << connect_error.message() << std::endl;
                }
            });
        _ioc.run();
        _ioc.stop();
        _ioc.reset();
        _tcpsocket.close();
        return gotparse;
    }

    bool UPNP::ParseURL(const std::string& url, std::string& host, unsigned short& port, std::string& path) {
        // Regular expression to match the URL format
        std::regex urlRegex(R"(http://([^:/]+):(\d+)(.*))");

        std::smatch match;
        if (std::regex_match(url, match, urlRegex)) {
            // Extract host, port, and path from the matched groups
            host = match[1];
            port = std::stoi(match[2]);
            path = match[3];
            return true;
        }
        else {
            std::cerr << "Invalid URL format: " << url << std::endl;
            return false;
        }
    }

    bool UPNP::ParseRootDesc(std::string& rootdesc)
    {
        //std::cout << "Root Desc: " << rootdesc << std::endl;
        std::istringstream iss(rootdesc);
        boost::property_tree::ptree tree;
        boost::property_tree::read_xml(iss, tree);
        //Get required values to upnp.
        try {
            auto deviceType = tree.get<std::string>("root.device.deviceType");
            auto friendlyName = tree.get<std::string>("root.device.friendlyName");
            auto manufacturer = tree.get<std::string>("root.device.manufacturer");
            auto modelDescription = tree.get<std::string>("root.device.modelDescription");

            // Process the retrieved values
            std::cout << "Device Type: " << deviceType << std::endl;
            std::cout << "Friendly Name: " << friendlyName << std::endl;
            std::cout << "Manufacturer: " << manufacturer << std::endl;
            std::cout << "Model Description: " << modelDescription << std::endl;

        }
        catch (const boost::property_tree::ptree_bad_path& ex) {
            std::cerr << "Error: " << ex.what() << std::endl;
        }
        std::cout << "Here" << std::endl;
        return false;
    }

    boost::optional<std::string> UPNP::getXMLValue(const boost::property_tree::ptree& tree, const std::string& path) {
        boost::optional<std::string> value;
        try {
            value = tree.get<std::string>(path);
        }
        catch (const std::exception& ex) {
            // Node not found or conversion error
        }
        return value;
    }

    bool UPNP::OpenPort(int intPort, int extPort, std::string type)
    {
        std::istringstream iss(_rootDescData);
        boost::property_tree::ptree tree;
        boost::property_tree::read_xml(iss, tree);
        std::string soap = "<?xml version=\"1.0\"?>"
            "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
            "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
            "<s:Body>"
            "<u:AddPortMapping xmlns:u=\"" + tree.get<std::string>("root.device.deviceList.device.deviceList.device.serviceList.service.serviceType") + "\">"
            "<NewRemoteHost></NewRemoteHost>"
            "<NewExternalPort>" + std::to_string(extPort) + "< / NewExternalPort>"
            "<NewProtocol>" + type + "</NewProtocol>"
            "<NewInternalPort>" + std::to_string(intPort) + "</NewInternalPort>"
            "<NewInternalClient>192.168.1.100</NewInternalClient>"
            "<NewEnabled>1</NewEnabled>"
            "<NewPortMappingDescription>SGNUS</NewPortMappingDescription>"
            "<NewLeaseDuration>60</NewLeaseDuration>"
            "</u:AddPortMapping>"
            "</s:Body>"
            "</s:Envelope>";
        std::cout << "Soap request " << soap << std::endl;
        return false;
    }
}