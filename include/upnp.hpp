/**
* Header file for upnp interactions(opening ports, getting external ip) 
* Uses boost asio (1.80.0) for operations.
* @author Justin Church
*/
#ifndef GNUS_UPNP
#define GNUS_UPNP
#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

namespace sgns::upnp
{

	class UPNP 
	{
	public:
		UPNP()
			: _ioc(), _socket(_ioc, boost::asio::ip::udp::v4()),
			_tcpsocket(_ioc, boost::asio::ip::tcp::v4()), _multicast(boost::asio::ip::address_v4({ 239, 255, 255, 250 }), 1900),
			_rootDescXML(), _rootDescData(), _localIpAddress(), _controlHost(""), _controlPort(0)
		{

		}
		/** Get IGD(Internet Gateway Device -- Router) info for future use
		* @return false on failure
		*/
		bool GetIGD();
		/** Open a port using upnp SOAP request 
		* @param intPort - Internal Port to open
		* @param extPort - External port to open
		* @param type - TCP/UDP
		* @param time - Time the port will stay open in seconds.
		* @return false on failure
		*/
		bool OpenPort(int intPort, int extPort, std::string type, int time);
		/** Get our wan IP
		* @return wan ip or nothing on failure
		*/
		std::string GetWanIP();
		/** Get our LAN IP
		* @return LAN ip or nothing on failure
		*/
		std::string GetLocalIP();
		/** List opened ports - not implemented(yet)
		*/
		void ListOpenPorts();

	private:
		/** Parse IGD data, currently only concerns itself with getting the rootDesc XML location
		* @param lines - var to store XML URL
		* @return false on failure
		*/
		bool ParseIGD(std::string& lines);
		/** Get the rootDesc.xml from the router, which describes devices and other data we need to interact with the router.
		* @param xml - rootDesc.xml URL
		* @return false on failure
		*/
		bool GetRootDesc(std::string xml);
		/** Parse URL for a host, port, and path
		* @param url - URL to parse
		* @param host - host name
		* @param port - port num
		* @param path - path to file
		* @return false on failure
		*/
		bool ParseURL(const std::string& url, std::string& host, unsigned short& port, std::string& path);
		/** Send a soap request to the IGD we obtained.
		* @param soaprq - Soap request (HTTP POST + XML data for SOAP)
		* @param result - Result returned from IGD
		* @return false on failure
		*/
		bool SendSOAPRequest(std::string soaprq, std::string& result);
		/** Adds HTTP headers to a SOAP xml data
		* @param soapxml - XML for soap request
		* @param path - Path to post to
		* @param device - Device parsed from rootDesc.xml we want to open port on, typically urn:schemas-upnp-org:service:WANIPConnection:#
		* @param action - Type of SOAP action we are performing, i.e. AddPortMapping or GetExternalIPAddress
		* @return final soap request string to send out
		*/
		std::string AddHTTPtoSoap(std::string soapxml, std::string path, std::string device, std::string action);

		//Vars
		boost::asio::io_context _ioc;
		boost::asio::ip::udp::socket _socket;
		boost::asio::ip::tcp::socket _tcpsocket;
		const boost::asio::ip::udp::endpoint _multicast;
		std::vector<std::string> _rootDescXML;
		std::string _rootDescData;
		std::string _localIpAddress;
		std::string _controlHost;
		int _controlPort;
	};
}

#endif