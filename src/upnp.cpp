#include <upnp.hpp>
#include <iostream>
#include <string>
#include <regex>

namespace sgns::upnp
{
    bool UPNP::GetIGD() {
        std::chrono::seconds timeout(2);
        std::array<const char*, 2> search_targets = {
            "urn:schemas-upnp-org:device:InternetGatewayDevice:1",
            "urn:schemas-upnp-org:device:InternetGatewayDevice:2"
        };
        auto self = shared_from_this();
        boost::asio::ip::udp::socket socket(*_ioc);

        boost::asio::ip::udp::resolver resolver(*_ioc);
        boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), boost::asio::ip::host_name(), "");

        // Iterate over each resolved endpoint
        for (auto endpoint_it = resolver.resolve(query); endpoint_it != boost::asio::ip::udp::resolver::iterator(); ++endpoint_it) {
            boost::asio::ip::udp::endpoint local_endpoint(*endpoint_it);

            // Open socket and bind to local endpoint
            socket.open(boost::asio::ip::udp::v4());
            socket.bind(local_endpoint);

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
                socket.async_send_to(boost::asio::buffer(request.data(), request.size()), _multicast,
                    [self, &socket, local_endpoint](const boost::system::error_code& error, size_t bytes_sent) {
                        if (!error) {
                            //std::array<char, 32 * 1024> rx;
                            //boost::asio::mutable_buffer receive_buffer(rx.data(), rx.size());
                            auto headerbuff = std::make_shared<boost::asio::streambuf>();

                            boost::asio::ip::udp::endpoint remote_endpoint;
                            socket.async_receive_from(headerbuff->prepare(1024), remote_endpoint,
                                [self, &socket, headerbuff, local_endpoint](const boost::system::error_code& receive_error, size_t bytes_received) {
                                    if (!receive_error) {
                                        headerbuff->commit(bytes_received);
                                        auto buffer = std::make_shared<std::vector<char>>(boost::asio::buffers_begin(headerbuff->data()), boost::asio::buffers_end(headerbuff->data()));
                                        std::string received_data(buffer->begin(), buffer->end());
                                        //std::cout << "Rec Data: " << received_data << std::endl;
                                        auto xmlavail = self->ParseIGD(local_endpoint.address().to_string(),received_data);
                                    }
                                    else {
                                        std::cerr << "Error receiving data: " << receive_error.message() << std::endl;
                                    }
                                });
                        }
                        else {
                            std::cerr << "Error sending data: " << error.message() << std::endl;
                        }
                    });
                // Create and start a timer for timeout
                boost::asio::deadline_timer timer(*_ioc);
                timer.expires_from_now(boost::posix_time::milliseconds(200));
                timer.async_wait([&](const boost::system::error_code& timer_error) {
                    std::cout << timer_error.message() << std::endl;
                    if (timer_error == boost::asio::error::operation_aborted) {
                        // Timer was cancelled due to successful completion
                        std::cerr << "Timer Cancelled Normally" << std::endl;
                    }
                    else {
                        // Timer expired, handle timeout
                        socket.cancel();
                        std::cerr << "Async operation timed out." << std::endl;
                    }
                    });
                _ioc->run();
                _ioc->reset();
                //_ioc->stop();
                
            }
            socket.close();
            std::cout << "End Req" << std::endl;
        }

        //socket.close();
        _ioc->stop();

        for (const auto& rootDescXML : *_rootDescXML) {
            auto gotrootdesc = GetRootDesc(rootDescXML);
            if (gotrootdesc) {
                std::cout << "Got root desc" << std::endl;
                return true;
            }
        }

        return false;
    }

    bool UPNP::ParseIGD(std::string ip, std::string lines)
    {
        std::cout << "Full IGD: " << lines << std::endl;
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
            std::cout << "String Location:" << match[1].str() << std::endl;
            return true;
        }
        else {
            // No LOCATION header found
            std::cout << "No header found" << std::endl;
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
        std::cout << "getrootdesc" << std::endl;
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(host), port);

        auto self = shared_from_this();

        //Connect 
        //TODO: Support HTTPS because some routers use this.
        //bool gotparse = false;
        auto gotparse = std::make_shared<bool>(false);
        //_ioc->reset();
        std::cout << "XML ADDR: " << xml.ipAddress << std::endl;
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
                    //std::cout << "1" << get_request << std::endl;
                    boost::asio::async_write(*tcpsocket, write_buffer, [self, tcpsocket, gotparse, host, port, write_buffer](const boost::system::error_code& write_error, std::size_t)
                        {
                            std::cout << "2" << std::endl;
                            //boost::asio::streambuf rootdesc;
                            auto rootdesc = std::make_shared<boost::asio::streambuf>();
                            boost::asio::async_read(*tcpsocket, *rootdesc, boost::asio::transfer_all(), [self, tcpsocket, gotparse, rootdesc, host, port](const boost::system::error_code& read_error, std::size_t bytes_transferred)
                                {
                                    std::cout << "3" << std::endl;
                                    auto buffer = std::vector<char>(boost::asio::buffers_begin(rootdesc->data()), boost::asio::buffers_end(rootdesc->data()));
                                    std::string bufferStr(buffer.begin(), buffer.end());
                                    auto bodyStartPos = bufferStr.find("\r\n\r\n");
                                    if (bodyStartPos != std::string::npos) {

                                        bufferStr = bufferStr.substr(bodyStartPos + 4);
                                        *self->_rootDescData = bufferStr;
                                        std::cout << "Root Description XML: " << self->_rootDescData << std::endl;
                                        //std::cout << "local endpoint test" << _tcpsocket.local_endpoint().address().to_string() << std::endl;
                                        self->_controlHost = host;
                                        self->_controlPort = port;
                                        self->_localIpAddress = tcpsocket->local_endpoint().address().to_string();
                                        //ParseRootDesc(bufferStr);
                                        *gotparse = true;
                                    }
                                    else {
                                        *gotparse = false;
                                        std::cerr << "Error Reading" << std::endl;
                                    }
                                });
                        });
                }
                else {
                    std::cerr << "Connection error: " << connect_error.message() << std::endl;
                }
            });
        std::cout << "Run Root Desc" << std::endl;
        _ioc->run();
        _ioc->stop();
        std::cout << "Root Desc Done" << std::endl;
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
            std::cerr << "Invalid URL format: " << url << std::endl;
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
        //std::cout << "Soap request " << soap << std::endl;
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
            std::cerr << "Property tree path error: " << e.what() << std::endl;
            return false;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
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
        std::cout << "Send Soap: " << soaprq << std::endl;
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
                    std::cout << "connected" << std::endl;
                    
                    boost::asio::async_write(*tcpsocket, write_buffer, [self, tcpsocket, write_buffer, &result](const boost::system::error_code& write_error, std::size_t)
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
                                        std::cout << "Soap request return" << bufferStr << std::endl;
                                    }
                                    else {
                                        std::cerr << "Error Reading" << std::endl;
                                    }
                                });
                        });
                }
                else {
                    std::cerr << "Connection error: " << connect_error.message() << std::endl;
                }
            });
        _ioc->run();
        std::cout << "Send Soap End" << std::endl;
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
        //std::cout << "Soap request " << soap << std::endl;
        auto soaprqwithhttp = AddHTTPtoSoap(soap, tree.get<std::string>("root.device.deviceList.device.deviceList.device.serviceList.service.controlURL"),
            tree.get<std::string>("root.device.deviceList.device.deviceList.device.serviceList.service.serviceType"),
            "#GetExternalIPAddress");
        

        //Send SOAP request
        std::string soapresponse;
        SendSOAPRequest(soaprqwithhttp, soapresponse);

        //Parse soap response for WAN IP
        //std::cout << "Wan IP Response before parse: " << soapresponse << std::endl;
        std::istringstream soaprespdata(soapresponse);
        boost::property_tree::ptree soaptree;
        boost::property_tree::read_xml(soaprespdata, soaptree);
        try {
            auto response_ip = soaptree.get<std::string>("s:Envelope.s:Body.u:GetExternalIPAddressResponse.NewExternalIPAddress");
            return response_ip;
        }
        catch (const boost::property_tree::ptree_bad_path& e) {
            std::cerr << "Property tree path error: " << e.what() << std::endl;
            return "";
        }
        catch (const std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
            return "";
        }
    }

    std::string UPNP::GetLocalIP()
    {
        return _localIpAddress;
    }
}