#include <any>
#include <upnp.hpp>
#include <upnp_igd_parser.hpp>
#include <upnp_util.hpp>
#include <iostream>
#include <string>
#include <regex>
#include <algorithm>
#include <set>
#if defined( _WIN32 )
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment( lib, "iphlpapi.lib" )
#pragma comment( lib, "ws2_32.lib" )
#else
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

namespace sgns::upnp
{
    std::vector<std::string> GetLocalIPv4CandidatesFromOS()
    {
        std::vector<std::string> candidates;
#if defined( _WIN32 )
        ULONG                 bufferSize       = 15000;
        IP_ADAPTER_ADDRESSES *adapterAddresses = (IP_ADAPTER_ADDRESSES *)malloc( bufferSize );

        if ( GetAdaptersAddresses( AF_INET, 0, nullptr, adapterAddresses, &bufferSize ) == ERROR_BUFFER_OVERFLOW )
        {
            free( adapterAddresses );
            adapterAddresses = (IP_ADAPTER_ADDRESSES *)malloc( bufferSize );
        }

        struct Candidate
        {
            std::string ip;
            bool        hasGateway;
            bool        isVirtualLike;
        };

        std::vector<Candidate> ranked;

        if ( GetAdaptersAddresses( AF_INET, GAA_FLAG_INCLUDE_GATEWAYS, nullptr, adapterAddresses, &bufferSize ) ==
             NO_ERROR )
        {
            for ( IP_ADAPTER_ADDRESSES *adapter = adapterAddresses; adapter; adapter = adapter->Next )
            {
                if ( adapter->OperStatus == IfOperStatusUp && adapter->IfType != IF_TYPE_SOFTWARE_LOOPBACK )
                {
                    const bool hasGateway    = ( adapter->FirstGatewayAddress != nullptr );
                    const bool isVirtualLike = ( adapter->IfType == IF_TYPE_TUNNEL ) ||
                                               ( adapter->IfType == IF_TYPE_PROP_VIRTUAL );

                    for ( IP_ADAPTER_UNICAST_ADDRESS *unicast = adapter->FirstUnicastAddress; unicast;
                          unicast                             = unicast->Next )
                    {
                        SOCKADDR *addrStruct = unicast->Address.lpSockaddr;
                        if ( addrStruct->sa_family == AF_INET )
                        { // Ensure it's IPv4
                            char buffer[INET_ADDRSTRLEN];
                            inet_ntop( AF_INET,
                                       &( ( (struct sockaddr_in *)addrStruct )->sin_addr ),
                                       buffer,
                                       INET_ADDRSTRLEN );
                            std::string localIP = buffer;

                            // Ensure it's not a loopback IP (127.x.x.x)
                            if ( localIP.rfind( "127.", 0 ) != 0 )
                            {
                                ranked.push_back( { localIP, hasGateway, isVirtualLike } );
                            }
                        }
                    }
                }
            }
        }

        free( adapterAddresses );

        std::stable_sort( ranked.begin(),
                          ranked.end(),
                          []( const Candidate &a, const Candidate &b )
                          {
                              if ( a.hasGateway != b.hasGateway )
                              {
                                  return a.hasGateway > b.hasGateway;
                              }
                              if ( a.isVirtualLike != b.isVirtualLike )
                              {
                                  return a.isVirtualLike < b.isVirtualLike;
                              }
                              return false;
                          } );

        for ( const auto &entry : ranked )
        {
            candidates.push_back( entry.ip );
        }

#else // Unix-like (Linux/macOS)
        struct ifaddrs *ifaddr, *ifa;

        if ( getifaddrs( &ifaddr ) == -1 )
        {
            perror( "getifaddrs" );
            return candidates;
        }

        for ( ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next )
        {
            if ( ifa->ifa_addr == nullptr )
            {
                continue;
            }

            int family = ifa->ifa_addr->sa_family;
            if ( family == AF_INET && !( ifa->ifa_flags & IFF_LOOPBACK ) )
            {
                char host[NI_MAXHOST];
                int  s = getnameinfo( ifa->ifa_addr,
                                     sizeof( struct sockaddr_in ),
                                     host,
                                     NI_MAXHOST,
                                     nullptr,
                                     0,
                                     NI_NUMERICHOST );
                if ( s == 0 )
                {
                    candidates.push_back( host );
                }
            }
        }

        freeifaddrs( ifaddr );
#endif

        return candidates;
    }

    bool UPNP::GetIGD()
    {
        m_logger->set_level( spdlog::level::trace );
        std::lock_guard<std::mutex> lock( upnp_mutex );
        _ioc->reset();

        std::chrono::seconds        timeout( 2 );
        std::array<const char *, 2> search_targets = { "urn:schemas-upnp-org:device:InternetGatewayDevice:1",
                                                       "urn:schemas-upnp-org:device:InternetGatewayDevice:2" };
        auto                        self           = shared_from_this();

        auto bindCandidates = GetLocalIPv4CandidatesFromOS();
        bindCandidates.push_back( "0.0.0.0" );

        for ( const auto &candidateIp : bindCandidates )
        {
            boost::asio::ip::udp::endpoint local_endpoint( boost::asio::ip::make_address( candidateIp ), 0 );
            try
            {
                if ( socket_->is_open() )
                {
                    socket_->close();
                }
                socket_->open( boost::asio::ip::udp::v4() );
                boost::asio::socket_base::reuse_address reuseAddr( true );
                socket_->set_option( reuseAddr );
                socket_->bind( local_endpoint );
                m_logger->info( "UPNP discovery bound to {}", local_endpoint.address().to_string() );
            }
            catch ( const boost::system::system_error &e )
            {
                m_logger->warn( "Failed to bind SSDP discovery socket on {}: {}", candidateIp, e.what() );
                continue;
            }

            for ( auto target : search_targets )
            {
                std::stringstream ss;
                ss << "M-SEARCH * HTTP/1.1\r\n"
                   << "HOST: 239.255.255.250:1900\r\n"
                   << "ST: " << target << "\r\n"
                   << "MAN: \"ssdp:discover\"\r\n"
                   << "MX: " << timeout.count() << "\r\n"
                   << "USER-AGENT: asio-upnp/1.0\r\n"
                   << "\r\n";
                auto request = std::make_shared<std::string>( ss.str() );
                auto timer   = std::make_shared<boost::asio::deadline_timer>( *_ioc );
                m_logger->trace( "Sending SSDP M-SEARCH for '{}' from {}",
                                 target,
                                 local_endpoint.address().to_string() );

                socket_->async_send_to(
                    boost::asio::buffer( *request ),
                    _multicast,
                    [self, local_endpoint, timer, request]( const boost::system::error_code &error, size_t bytes_sent )
                    {
                        if ( !error )
                        {
                            auto headerbuff      = std::make_shared<boost::asio::streambuf>();
                            auto remote_endpoint = std::make_shared<boost::asio::ip::udp::endpoint>();
                            auto head_begin      = headerbuff->prepare( 1024 );

                            self->socket_->async_receive_from(
                                head_begin,
                                *remote_endpoint,
                                [self, headerbuff, local_endpoint, timer](
                                    const boost::system::error_code &receive_error,
                                    size_t                           bytes_received )
                                {
                                    if ( !receive_error )
                                    {
                                        timer->cancel();
                                        headerbuff->commit( bytes_received );
                                        auto buffer = std::make_shared<std::vector<char>>(
                                            boost::asio::buffers_begin( headerbuff->data() ),
                                            boost::asio::buffers_end( headerbuff->data() ) );
                                        std::string received_data( buffer->begin(), buffer->end() );
                                        auto        xmlavail = self->ParseIGD( local_endpoint.address().to_string(),
                                                                        received_data );
                                    }
                                    else if ( receive_error != boost::asio::error::operation_aborted )
                                    {
                                        self->m_logger->error( "Error receiving data: {}", receive_error.message() );
                                    }
                                } );
                        }
                        else
                        {
                            self->m_logger->error( "Error sending data: {}", error.message() );
                        }
                    } );

                timer->expires_from_now( boost::posix_time::milliseconds( timeout.count() * 1000 ) );
                timer->async_wait(
                    [self]( const boost::system::error_code &timer_error )
                    {
                        if ( !timer_error )
                        {
                            self->socket_->cancel();
                            self->m_logger->debug( "SSDP receive timed out waiting for response." );
                        }
                    } );

                _ioc->run();
                _ioc->reset();

                if ( !_rootDescXML->empty() )
                {
                    break;
                }
            }

            socket_->close();
            if ( !_rootDescXML->empty() )
            {
                break;
            }
        }

        //socket_->close();
        if ( _rootDescXML->empty() )
        {
            m_logger->warn( "No SSDP responses received from any local interface." );
        }

        // Deduplicate: some devices (e.g. Philips Hue) respond multiple
        // times to the same M-SEARCH.  Only fetch each rootDesc URL once.
        {
            std::set<std::string> seen;
            auto                  it = _rootDescXML->begin();
            while ( it != _rootDescXML->end() )
            {
                if ( !seen.insert( it->rootDescXML ).second )
                {
                    m_logger->debug( "Skipping duplicate SSDP response from {}", it->rootDescXML );
                    it = _rootDescXML->erase( it );
                }
                else
                {
                    ++it;
                }
            }
            m_logger->info( "SSDP discovery: {} unique device(s) responded", _rootDescXML->size() );
        }

        for ( const auto &rootDescXML : *_rootDescXML )
        {
            auto gotrootdesc = GetRootDesc( rootDescXML );
            if ( gotrootdesc )
            {
                self->m_logger->info( "Got root desc from IGD." );
                return true;
            }
        }

        return false;
    }

    bool UPNP::ParseIGD( std::string ip, std::string lines )
    {
        auto parsed = parseSSDPResponse( lines, ip );
        if ( parsed )
        {
            IGDInfo info;
            info.ipAddress   = parsed->ipAddress;
            info.rootDescXML = parsed->locationURL;
            _rootDescXML->push_back( info );
            return true;
        }
        m_logger->error( "No location header from IGD." );
        return false;
    }

    bool UPNP::GetRootDesc( IGDInfo xml )
    {
        //Parse URL to get correct elements for HTTP get and entpoint.
        std::string    host;
        unsigned short port;
        std::string    path;

        if ( !ParseURL( xml.rootDescXML, host, port, path ) )
        {
            return false;
        }
        boost::asio::ip::tcp::endpoint endpoint( boost::asio::ip::address::from_string( host ), port );

        auto self = shared_from_this();

        //Connect
        //TODO: Support HTTPS because some routers use this.
        auto                           gotparse = std::make_shared<bool>( false );
        boost::asio::ip::tcp::endpoint tcp_endpoint( boost::asio::ip::address_v4::any(), 0 );
        auto                           tcpsocket = std::make_shared<boost::asio::ip::tcp::socket>( *_ioc );
        tcpsocket->open( boost::asio::ip::tcp::v4() );
        tcpsocket->bind( tcp_endpoint );
        std::string get_request  = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
        auto        write_buffer = boost::asio::buffer( get_request );
        tcpsocket->async_connect(
            endpoint,
            [self, tcpsocket, gotparse, host, port, write_buffer]( const boost::system::error_code &connect_error )
            {
                if ( !connect_error )
                {
                    //auto write_buffer = boost::asio::buffer(get_request);
                    boost::asio::async_write(
                        *tcpsocket,
                        write_buffer,
                        [self, tcpsocket, gotparse, host, port]( const boost::system::error_code &write_error,
                                                                 std::size_t )
                        {
                            //boost::asio::streambuf rootdesc;
                            auto rootdesc = std::make_shared<boost::asio::streambuf>();
                            boost::asio::async_read(
                                *tcpsocket,
                                *rootdesc,
                                boost::asio::transfer_all(),
                                [self, tcpsocket, gotparse, rootdesc, host, port](
                                    const boost::system::error_code &read_error,
                                    std::size_t                      bytes_transferred )
                                {
                                    auto buffer = std::vector<char>( boost::asio::buffers_begin( rootdesc->data() ),
                                                                     boost::asio::buffers_end( rootdesc->data() ) );
                                    std::string bufferStr( buffer.begin(), buffer.end() );
                                    auto        bodyStartPos = bufferStr.find( "\r\n\r\n" );
                                    if ( bodyStartPos != std::string::npos )
                                    {
                                        bufferStr             = bufferStr.substr( bodyStartPos + 4 );
                                        *self->_rootDescData  = bufferStr;
                                        self->_controlHost    = host;
                                        self->_controlPort    = port;
                                        self->_localIpAddress = tcpsocket->local_endpoint().address().to_string();
                                        *self->_bindIp        = self->_localIpAddress;
                                        self->m_logger->info( "UPnP root description received from {}:{} ({} bytes, "
                                                              "first 500 chars): {}",
                                                              host,
                                                              port,
                                                              bufferStr.size(),
                                                              bufferStr.substr( 0, 500 ) );
                                        *gotparse = true;
                                    }
                                    else
                                    {
                                        *gotparse = false;
                                        self->m_logger->error( "Error Reading root desc" );
                                    }
                                } );
                        } );
                }
                else
                {
                    self->m_logger->error( "Connection Error: {}", connect_error.message() );
                }
            } );
        _ioc->run();
        _ioc->stop();
        _ioc->reset();

        tcpsocket->close();

        // Verify this is actually an InternetGatewayDevice, not some other
        // UPnP device (e.g. Philips Hue Bridge) that responded to M-SEARCH.
        if ( *gotparse )
        {
            try
            {
                std::istringstream          iss( *_rootDescData );
                boost::property_tree::ptree tree;
                boost::property_tree::read_xml( iss, tree );
                auto deviceType = tree.get<std::string>( "root.device.deviceType" );

                if ( deviceType.find( "InternetGatewayDevice" ) == std::string::npos )
                {
                    m_logger->warn( "SSDP response from {}:{} is not an IGD (deviceType: {}), "
                                    "continuing discovery...",
                                    host,
                                    port,
                                    deviceType );
                    _rootDescData->clear();
                    return false;
                }
            }
            catch ( const boost::property_tree::ptree_bad_path &e )
            {
                m_logger->warn( "Cannot determine device type from {}:{}, continuing "
                                "discovery...",
                                host,
                                port );
                _rootDescData->clear();
                return false;
            }
            catch ( const std::exception &e )
            {
                m_logger->warn( "Error parsing root description from {}:{}: {}", host, port, e.what() );
                _rootDescData->clear();
                return false;
            }
        }

        return *gotparse;
    }

    bool UPNP::ParseURL( const std::string &url, std::string &host, unsigned short &port, std::string &path )
    {
        if ( !sgns::upnp::ParseURL( url, host, port, path ) )
        {
            m_logger->error( "Invalid URL format: {}", url );
            return false;
        }
        return true;
    }

    bool UPNP::OpenPort( int intPort, int extPort, std::string type, int time )
    {
        //Read XML data
        std::istringstream          iss( *_rootDescData );
        boost::property_tree::ptree tree;
        boost::property_tree::read_xml( iss, tree );

        // Parse service info safely — routers may have different XML structures
        auto svc = parseIGDServiceInfo( tree, *_rootDescData, m_logger );
        if ( !svc )
        {
            return false;
        }

        //Build a SOAP request
        std::string soap = "<?xml version=\"1.0\"?>"
                           "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
                           " s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
                           "<s:Body>"
                           "<u:AddPortMapping xmlns:u=\"" +
                           svc->serviceType +
                           "\">"
                           "<NewRemoteHost></NewRemoteHost>"
                           "<NewExternalPort>" +
                           std::to_string( extPort ) +
                           "</NewExternalPort>"
                           "<NewProtocol>" +
                           type +
                           "</NewProtocol>"
                           "<NewInternalPort>" +
                           std::to_string( intPort ) +
                           "</NewInternalPort>"
                           "<NewInternalClient>" +
                           _localIpAddress +
                           "</NewInternalClient>"
                           "<NewEnabled>1</NewEnabled>"
                           "<NewPortMappingDescription>SGNUS</NewPortMappingDescription>"
                           "<NewLeaseDuration>" +
                           std::to_string( time ) +
                           "</NewLeaseDuration>"
                           "</u:AddPortMapping>"
                           "</s:Body>"
                           "</s:Envelope>";
        auto soaprqwithhttp = AddHTTPtoSoap( soap, svc->controlURL, svc->serviceType, "#AddPortMapping" );

        //Send SOAP request
        std::string soapresponse;
        SendSOAPRequest( soaprqwithhttp, soapresponse );

        //Parse SOAP request for success
        std::istringstream          soaprespdata( soapresponse );
        boost::property_tree::ptree soaptree;
        boost::property_tree::read_xml( soaprespdata, soaptree );
        try
        {
            auto err_check = soaptree.get<std::string>( "s:Envelope.s:Body.u:AddPortMappingResponse" );
            return true;
        }
        catch ( const boost::property_tree::ptree_bad_path &e )
        {
            m_logger->error( "Property tree path error: {}", e.what() );
            return false;
        }
        catch ( const std::exception &e )
        {
            m_logger->error( "Exception: {}", e.what() );
            return false;
        }
    }

    std::string UPNP::AddHTTPtoSoap( std::string soapxml, std::string path, std::string device, std::string action )
    {
        return sgns::upnp::AddHTTPtoSoap( std::move( soapxml ),
                                          std::move( path ),
                                          _controlHost,
                                          _controlPort,
                                          std::move( device ),
                                          std::move( action ) );
    }

    bool UPNP::SendSOAPRequest( std::string soaprq, std::string &result )
    {
        auto self = shared_from_this();
        //Get Router IP
        boost::asio::ip::tcp::endpoint endpoint( boost::asio::ip::address::from_string( _controlHost ), _controlPort );

        auto tcpsocket = std::make_shared<boost::asio::ip::tcp::socket>( *_ioc );
        tcpsocket->open( boost::asio::ip::tcp::v4() );

        // Prefer binding to the discovered local interface; fall back to all interfaces.
        try
        {
            if ( !_bindIp->empty() && *_bindIp != "0.0.0.0" )
            {
                boost::asio::ip::tcp::endpoint tcp_endpoint( boost::asio::ip::make_address( *_bindIp ), 0 );
                tcpsocket->bind( tcp_endpoint );
            }
            else
            {
                tcpsocket->bind( boost::asio::ip::tcp::endpoint( boost::asio::ip::address_v4::any(), 0 ) );
            }
        }
        catch ( const std::exception &e )
        {
            m_logger->warn( "Unable to bind SOAP socket to preferred local IP ({}). Falling back to OS routing.",
                            e.what() );
        }
        //Make Soap Buffer
        auto write_buffer = boost::asio::buffer( soaprq );
        tcpsocket->async_connect(
            endpoint,
            [self, tcpsocket, write_buffer, &result]( const boost::system::error_code &connect_error )
            {
                if ( !connect_error )
                {
                    boost::asio::async_write(
                        *tcpsocket,
                        write_buffer,
                        [self, tcpsocket, &result]( const boost::system::error_code &write_error, std::size_t )
                        {
                            auto openportb = std::make_shared<boost::asio::streambuf>();
                            boost::asio::async_read(
                                *tcpsocket,
                                *openportb,
                                boost::asio::transfer_all(),
                                [self, tcpsocket, openportb, &result]( const boost::system::error_code &read_error,
                                                                       std::size_t bytes_transferred )
                                {
                                    auto buffer = std::vector<char>( boost::asio::buffers_begin( openportb->data() ),
                                                                     boost::asio::buffers_end( openportb->data() ) );
                                    std::string bufferStr( buffer.begin(), buffer.end() );
                                    auto        bodyStartPos = bufferStr.find( "\r\n\r\n" );
                                    if ( bodyStartPos != std::string::npos )
                                    {
                                        bufferStr = bufferStr.substr( bodyStartPos + 4 );
                                        result    = bufferStr;
                                    }
                                    else
                                    {
                                        self->m_logger->error( "Error Reading soap request result" );
                                    }
                                } );
                        } );
                }
                else
                {
                    self->m_logger->error( "Connection error: {}", connect_error.message() );
                }
            } );
        _ioc->run();
        _ioc->stop();
        _ioc->reset();
        tcpsocket->close();
        return true;
    }

    std::string UPNP::GetWanIP()
    {
        //Read XML data
        std::istringstream          iss( *_rootDescData );
        boost::property_tree::ptree tree;
        boost::property_tree::read_xml( iss, tree );

        // Parse service info safely — routers may have different XML structures
        auto svc = parseIGDServiceInfo( tree, *_rootDescData, m_logger );
        if ( !svc )
        {
            return "";
        }

        //Build a SOAP request
        std::string soap = "<?xml version=\"1.0\"?>"
                           "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
                           " s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
                           "<s:Body>"
                           "<u:GetExternalIPAddress xmlns:u=\"" +
                           svc->serviceType +
                           "\">"
                           "</u:GetExternalIPAddress>"
                           "</s:Body>"
                           "</s:Envelope>";
        auto soaprqwithhttp = AddHTTPtoSoap( soap, svc->controlURL, svc->serviceType, "#GetExternalIPAddress" );

        //Send SOAP request
        std::string soapresponse;
        SendSOAPRequest( soaprqwithhttp, soapresponse );

        //Parse soap response for WAN IP
        std::istringstream          soaprespdata( soapresponse );
        boost::property_tree::ptree soaptree;
        boost::property_tree::read_xml( soaprespdata, soaptree );
        try
        {
            auto response_ip = soaptree.get<std::string>(
                "s:Envelope.s:Body.u:GetExternalIPAddressResponse.NewExternalIPAddress" );
            return response_ip;
        }
        catch ( const boost::property_tree::ptree_bad_path &e )
        {
            m_logger->error( "Property tree path error: {}", e.what() );
            return "";
        }
        catch ( const std::exception &e )
        {
            m_logger->error( "Exception: {}", e.what() );
            return "";
        }
    }

    std::string UPNP::GetLocalIP()
    {
        return _localIpAddress;
    }

    bool UPNP::CheckIfPortInUse( int extPort, const std::string &protocol, std::string &outInternalClient )
    {
        // Read IGD XML
        std::istringstream          iss( *_rootDescData );
        boost::property_tree::ptree tree;
        boost::property_tree::read_xml( iss, tree );

        // Parse service info safely — routers may have different XML structures
        auto svc = parseIGDServiceInfo( tree, *_rootDescData, m_logger );
        if ( !svc )
        {
            return false;
        }

        // Build SOAP request for GetSpecificPortMappingEntry
        std::string soap = "<?xml version=\"1.0\"?>"
                           "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
                           " s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
                           "<s:Body>"
                           "<u:GetSpecificPortMappingEntry xmlns:u=\"" +
                           svc->serviceType +
                           "\">"
                           "<NewRemoteHost></NewRemoteHost>"
                           "<NewExternalPort>" +
                           std::to_string( extPort ) +
                           "</NewExternalPort>"
                           "<NewProtocol>" +
                           protocol +
                           "</NewProtocol>"
                           "</u:GetSpecificPortMappingEntry>"
                           "</s:Body>"
                           "</s:Envelope>";

        // Add HTTP headers to SOAP body
        auto soaprqwithhttp = AddHTTPtoSoap( soap, svc->controlURL, svc->serviceType, "#GetSpecificPortMappingEntry" );

        // Send SOAP request and get result
        std::string soapresponse;
        if ( !SendSOAPRequest( soaprqwithhttp, soapresponse ) )
        {
            return false; // Network error
        }

        // Parse SOAP response
        std::istringstream          soaprespdata( soapresponse );
        boost::property_tree::ptree soaptree;
        boost::property_tree::read_xml( soaprespdata, soaptree );

        try
        {
            outInternalClient = soaptree.get<std::string>(
                "s:Envelope.s:Body.u:GetSpecificPortMappingEntryResponse.NewInternalClient" );
            return true;
        }
        catch ( const boost::property_tree::ptree_bad_path &e )
        {
            // If the port mapping entry does not exist, this will throw
            m_logger->info( "Port {} (protocol {}) is not mapped: {}", extPort, protocol, e.what() );
            return false;
        }
        catch ( const std::exception &e )
        {
            m_logger->error( "Exception while checking port {} ({}): {}", extPort, protocol, e.what() );
            return false;
        }
    }
}
