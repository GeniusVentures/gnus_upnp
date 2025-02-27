#include <upnp.hpp>
#include <iostream>
#include <string>
#include <regex>
#if defined(_WIN32)
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

namespace sgns::upnp
{
    std::string GetLocalIPFromOS() {
#if defined(_WIN32)
        ULONG bufferSize = 15000;
        IP_ADAPTER_ADDRESSES* adapterAddresses = (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);

        if (GetAdaptersAddresses(AF_INET, 0, nullptr, adapterAddresses, &bufferSize) == ERROR_BUFFER_OVERFLOW) {
            free(adapterAddresses);
            adapterAddresses = (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);
        }

        std::string localIP = "";  // Start with an empty string

        if (GetAdaptersAddresses(AF_INET, 0, nullptr, adapterAddresses, &bufferSize) == NO_ERROR) {
            for (IP_ADAPTER_ADDRESSES* adapter = adapterAddresses; adapter; adapter = adapter->Next) {
                if (adapter->OperStatus == IfOperStatusUp && adapter->IfType != IF_TYPE_SOFTWARE_LOOPBACK) {
                    for (IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
                        SOCKADDR* addrStruct = unicast->Address.lpSockaddr;
                        if (addrStruct->sa_family == AF_INET) { // Ensure it's IPv4
                            char buffer[INET_ADDRSTRLEN];
                            inet_ntop(AF_INET, &(((struct sockaddr_in*)addrStruct)->sin_addr), buffer, INET_ADDRSTRLEN);
                            localIP = buffer;

                            // Ensure it's not a loopback IP (127.x.x.x)
                            if (localIP.rfind("127.", 0) != 0) {
                                free(adapterAddresses);
                                return localIP;
                            }
                        }
                    }
                }
            }
        }

        free(adapterAddresses);
        return localIP;  // Return the best found address or empty if none were valid

#else  // Unix-like (Linux/macOS)
        struct ifaddrs* ifaddr, * ifa;
        std::string localIP = "";

        if (getifaddrs(&ifaddr) == -1) {
            perror("getifaddrs");
            return localIP;
        }

        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) continue;

            int family = ifa->ifa_addr->sa_family;
            if (family == AF_INET && !(ifa->ifa_flags & IFF_LOOPBACK)) {
                char host[NI_MAXHOST];
                int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
                if (s == 0) {
                    localIP = host;
                    break;  // Return the first valid non-loopback IPv4 address
                }
            }
        }

        freeifaddrs(ifaddr);
        return localIP;  // Return the best found address or empty if none were valid
#endif
    }

    bool UPNP::GetIGD() {
        std::lock_guard<std::mutex> lock(upnp_mutex);

        std::chrono::seconds timeout(2);
        std::array<const char*, 2> search_targets = {
            "urn:schemas-upnp-org:device:InternetGatewayDevice:1",
            "urn:schemas-upnp-org:device:InternetGatewayDevice:2"
        };
        auto self = shared_from_this();

        std::string local_ip = GetLocalIPFromOS();
        if (local_ip.empty()) {
            return false;
        }
        // Iterate over each resolved endpoint
        //for (auto endpoint_it = resolver.resolve(query); endpoint_it != boost::asio::ip::udp::resolver::iterator(); ++endpoint_it) {
            //boost::asio::ip::udp::endpoint local_endpoint(boost::asio::ip::address_v4::any(), 0);
        boost::asio::ip::udp::endpoint local_endpoint(boost::asio::ip::make_address(local_ip), 0);
        try {
            
            //boost::asio::ip::udp::endpoint local_endpoint(*endpoint_it);

            // Open socket and bind to local endpoint
            socket_->open(boost::asio::ip::udp::v4());
            boost::asio::socket_base::reuse_address reuseAddr(true);
            socket_->set_option(reuseAddr);
            socket_->bind(local_endpoint);
        }
        catch (const boost::system::system_error& e) {
            m_logger->error("Failed to bind to {} because {}", local_ip, e.what());
            return false;
        }
            // Send M-SEARCH request for each search target
            for (auto target : search_targets) {
                std::stringstream ss;
                ss << "M-SEARCH * HTTP/1.1\r\n"
                    << "HOST: 239.255.255.250:1900\r\n"  // Multicast address and port
                    << "ST: " << target << "\r\n"
                    << "MAN: \"ssdp:discover\"\r\n"
                    << "MX: " << timeout.count() << "\r\n"
                    << "USER-AGENT: asio-upnp/1.0\r\n"
                    << "\r\n";
                std::string request = ss.str();
                socket_->async_send_to(boost::asio::buffer(request.data(), request.size()), _multicast,
                    [self, local_endpoint](const boost::system::error_code& error, size_t bytes_sent) {
                        if (!error) {
                            //std::array<char, 32 * 1024> rx;
                            //boost::asio::mutable_buffer receive_buffer(rx.data(), rx.size());
                            auto headerbuff = std::make_shared<boost::asio::streambuf>();
                            auto head_begin = headerbuff->prepare(1024);

                            boost::asio::ip::udp::endpoint remote_endpoint;
                            self->socket_->async_receive_from(head_begin, remote_endpoint,
                                [self, headerbuff, local_endpoint](const boost::system::error_code& receive_error, size_t bytes_received) {
                                    if (!receive_error) {
                                        headerbuff->commit(bytes_received);
                                        auto buffer = std::make_shared<std::vector<char>>(boost::asio::buffers_begin(headerbuff->data()), boost::asio::buffers_end(headerbuff->data()));
                                        std::string received_data(buffer->begin(), buffer->end());
                                        auto xmlavail = self->ParseIGD(local_endpoint.address().to_string(),received_data);
                                    }
                                    else {
                                        self->m_logger->error("Error receiving data:  {}", receive_error.message());
                                    }
                                });
                        }
                        else {
                            self->m_logger->error("Error sending data:  {}", error.message());
                        }
                    });
                // Create and start a timer for timeout
                boost::asio::deadline_timer timer(*_ioc);
                timer.expires_from_now(boost::posix_time::milliseconds(200));
                timer.async_wait([&](const boost::system::error_code& timer_error) {
                    if (timer_error == boost::asio::error::operation_aborted) {
                        // Timer was cancelled due to successful completion
                        self->m_logger->info("Timer Cancelled Normally");
                    }
                    else {
                        // Timer expired, handle timeout
                        socket_->cancel();
                        self->m_logger->error("Async operation timed out.");
                    }
                    });
                _ioc->run();
                _ioc->reset();
                //_ioc->stop();
                
            }
            socket_->close();
        //}

        //socket_->close();
        _ioc->stop();

        for (const auto& rootDescXML : *_rootDescXML) {
            auto gotrootdesc = GetRootDesc(rootDescXML);
            if (gotrootdesc) {
                self->m_logger->info("Got root desc from IGD.");
                return true;
            }
        }

        return false;
    }

    bool UPNP::ParseIGD(std::string ip, std::string lines)
    {
        // Define a regex pattern to match the LOCATION header
        std::regex locationRegex(R"(LOCATION:\s*(.*))", std::regex_constants::icase);

        // Search for the LOCATION header in the response
        std::smatch match;
        if (std::regex_search(lines, match, locationRegex)) {
            // Extract and return the location URL
            IGDInfo info;
            info.ipAddress = ip;
            info.rootDescXML = match[1].str();
            _rootDescXML->push_back(info);
            return true;
        }
        else {
            // No LOCATION header found
            m_logger->error("No location header from IGD.");
            return false;
        }
    }

    bool UPNP::GetRootDesc(IGDInfo xml)
    {
        //Parse URL to get correct elements for HTTP get and entpoint.
        std::string host;
        unsigned short port;
        std::string path;
        
        if (!ParseURL(xml.rootDescXML, host, port, path))
        {
            return false;
        }
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(host), port);

        auto self = shared_from_this();

        //Connect 
        //TODO: Support HTTPS because some routers use this.
        auto gotparse = std::make_shared<bool>(false);
        boost::asio::ip::tcp::endpoint tcp_endpoint(boost::asio::ip::make_address(xml.ipAddress), 0);
        *_bindIp = xml.ipAddress;
        auto tcpsocket = std::make_shared<boost::asio::ip::tcp::socket>(*_ioc);
        tcpsocket->open(boost::asio::ip::tcp::v4());
        tcpsocket->bind(tcp_endpoint);
        std::string get_request = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
        auto write_buffer = boost::asio::buffer(get_request);
        tcpsocket->async_connect(endpoint, [self, tcpsocket, gotparse, host, port, write_buffer](const boost::system::error_code& connect_error)
            {
                if (!connect_error)
                {
                    //auto write_buffer = boost::asio::buffer(get_request);
                    boost::asio::async_write(*tcpsocket, write_buffer, [self, tcpsocket, gotparse, host, port](const boost::system::error_code& write_error, std::size_t)
                        {
                            //boost::asio::streambuf rootdesc;
                            auto rootdesc = std::make_shared<boost::asio::streambuf>();
                            boost::asio::async_read(*tcpsocket, *rootdesc, boost::asio::transfer_all(), [self, tcpsocket, gotparse, rootdesc, host, port](const boost::system::error_code& read_error, std::size_t bytes_transferred)
                                {
                                    auto buffer = std::vector<char>(boost::asio::buffers_begin(rootdesc->data()), boost::asio::buffers_end(rootdesc->data()));
                                    std::string bufferStr(buffer.begin(), buffer.end());
                                    auto bodyStartPos = bufferStr.find("\r\n\r\n");
                                    if (bodyStartPos != std::string::npos) {

                                        bufferStr = bufferStr.substr(bodyStartPos + 4);
                                        *self->_rootDescData = bufferStr;
                                        self->_controlHost = host;
                                        self->_controlPort = port;
                                        self->_localIpAddress = tcpsocket->local_endpoint().address().to_string();
                                        //ParseRootDesc(bufferStr);
                                        *gotparse = true;
                                    }
                                    else {
                                        *gotparse = false;
                                        self->m_logger->error("Error Reading root desc");
                                    }
                                });
                        });
                }
                else {
                    self->m_logger->error("Connection Error: {}", connect_error.message());
                }
            });
        _ioc->run();
        _ioc->stop();
        _ioc->reset();
        
        tcpsocket->close();
        return *gotparse;
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
            m_logger->error("Invalid URL format: {}", url);
            return false;
        }
    }

    bool UPNP::OpenPort(int intPort, int extPort, std::string type, int time)
    {
        //Read XML data
        std::istringstream iss(*_rootDescData);
        boost::property_tree::ptree tree;
        boost::property_tree::read_xml(iss, tree);
        //Build a SOAP request
        std::string soap = "<?xml version=\"1.0\"?>"
            "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
            " s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
            "<s:Body>"
            "<u:AddPortMapping xmlns:u=\"" + tree.get<std::string>("root.device.deviceList.device.deviceList.device.serviceList.service.serviceType") + "\">"
            "<NewRemoteHost></NewRemoteHost>"
            "<NewExternalPort>" + std::to_string(extPort) + "</NewExternalPort>"
            "<NewProtocol>" + type + "</NewProtocol>"
            "<NewInternalPort>" + std::to_string(intPort) + "</NewInternalPort>"
            "<NewInternalClient>" + _localIpAddress + "</NewInternalClient>"
            "<NewEnabled>1</NewEnabled>"
            "<NewPortMappingDescription>SGNUS</NewPortMappingDescription>"
            "<NewLeaseDuration>" + std::to_string(time) + "</NewLeaseDuration>"
            "</u:AddPortMapping>"
            "</s:Body>"
            "</s:Envelope>";
        auto soaprqwithhttp = AddHTTPtoSoap(soap, tree.get<std::string>("root.device.deviceList.device.deviceList.device.serviceList.service.controlURL"),
            tree.get<std::string>("root.device.deviceList.device.deviceList.device.serviceList.service.serviceType"),
            "#AddPortMapping");

        //Send SOAP request
        std::string soapresponse;
        SendSOAPRequest(soaprqwithhttp, soapresponse);

        //Parse SOAP request for success
        std::istringstream soaprespdata(soapresponse);
        boost::property_tree::ptree soaptree;
        boost::property_tree::read_xml(soaprespdata, soaptree);
        try {
            auto err_check = soaptree.get<std::string>("s:Envelope.s:Body.u:AddPortMappingResponse");
            return true;
        }
        catch (const boost::property_tree::ptree_bad_path& e) {
            m_logger->error("Property tree path error: {}", e.what());
            return false;
        }
        catch (const std::exception& e) {
            m_logger->error("Exception: {}", e.what());
            return false;
        }
    }

    std::string UPNP::AddHTTPtoSoap(std::string soapxml, std::string path, std::string device, std::string action)
    {
        //Construct a SOAP request with HTTP header for posting
        std::string httpRequest = "POST " + path + " HTTP/1.1\r\n";
        httpRequest += "HOST: " + _controlHost + ":" + std::to_string(_controlPort) + "\r\n";
        httpRequest += "CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n";
        httpRequest += "CONTENT-LENGTH: " + std::to_string(soapxml.size()) + "\r\n";
        httpRequest += "SOAPACTION: \"" + device + action + "\"\r\n";
        httpRequest += "\r\n"; // End of headers
        httpRequest += soapxml;
        return httpRequest;
    }

    bool UPNP::SendSOAPRequest(std::string soaprq, std::string& result)
    {
        auto self = shared_from_this();
        //Get Router IP
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(_controlHost), _controlPort);

        //Bind to the ip we got IGD from
        boost::asio::ip::tcp::endpoint tcp_endpoint(boost::asio::ip::make_address(*_bindIp), 0);
        auto tcpsocket = std::make_shared<boost::asio::ip::tcp::socket>(*_ioc);
        tcpsocket->open(boost::asio::ip::tcp::v4());
        tcpsocket->bind(tcp_endpoint);
        //Make Soap Buffer
        auto write_buffer = boost::asio::buffer(soaprq);
        tcpsocket->async_connect(endpoint, [self, tcpsocket, write_buffer, &result](const boost::system::error_code& connect_error)
            {
                if (!connect_error)
                {                    
                    boost::asio::async_write(*tcpsocket, write_buffer, [self, tcpsocket, &result](const boost::system::error_code& write_error, std::size_t)
                        {
                            auto openportb = std::make_shared<boost::asio::streambuf>();
                            boost::asio::async_read(*tcpsocket, *openportb, boost::asio::transfer_all(), [self, tcpsocket, openportb, &result](const boost::system::error_code& read_error, std::size_t bytes_transferred)
                                {
                                    auto buffer = std::vector<char>(boost::asio::buffers_begin(openportb->data()), boost::asio::buffers_end(openportb->data()));
                                    std::string bufferStr(buffer.begin(), buffer.end());
                                    auto bodyStartPos = bufferStr.find("\r\n\r\n");
                                    if (bodyStartPos != std::string::npos) {

                                        bufferStr = bufferStr.substr(bodyStartPos + 4);
                                        result = bufferStr;
                                    }
                                    else {
                                        self->m_logger->error("Error Reading soap request result");
                                    }
                                });
                        });
                }
                else {
                    self->m_logger->error("Connection error: {}", connect_error.message());
                }
            });
        _ioc->run();
        _ioc->stop();
        _ioc->reset();
        tcpsocket->close();
        return true;
    }

    std::string UPNP::GetWanIP()
    {
        //Read XML data
        std::istringstream iss(*_rootDescData);
        boost::property_tree::ptree tree;
        boost::property_tree::read_xml(iss, tree);

        //Build a SOAP request
        std::string soap = "<?xml version=\"1.0\"?>"
            "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
            " s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
            "<s:Body>"
            "<u:GetExternalIPAddress xmlns:u=\"" + tree.get<std::string>("root.device.deviceList.device.deviceList.device.serviceList.service.serviceType") + "\">"
            "</u:GetExternalIPAddress>"
            "</s:Body>"
            "</s:Envelope>";
        auto soaprqwithhttp = AddHTTPtoSoap(soap, tree.get<std::string>("root.device.deviceList.device.deviceList.device.serviceList.service.controlURL"),
            tree.get<std::string>("root.device.deviceList.device.deviceList.device.serviceList.service.serviceType"),
            "#GetExternalIPAddress");
        

        //Send SOAP request
        std::string soapresponse;
        SendSOAPRequest(soaprqwithhttp, soapresponse);

        //Parse soap response for WAN IP
        std::istringstream soaprespdata(soapresponse);
        boost::property_tree::ptree soaptree;
        boost::property_tree::read_xml(soaprespdata, soaptree);
        try {
            auto response_ip = soaptree.get<std::string>("s:Envelope.s:Body.u:GetExternalIPAddressResponse.NewExternalIPAddress");
            return response_ip;
        }
        catch (const boost::property_tree::ptree_bad_path& e) {
            m_logger->error("Property tree path error: {}", e.what());
            return "";
        }
        catch (const std::exception& e) {
            m_logger->error("Exception: {}", e.what());
            return "";
        }
    }

    std::string UPNP::GetLocalIP()
    {
        return _localIpAddress;
    }
}