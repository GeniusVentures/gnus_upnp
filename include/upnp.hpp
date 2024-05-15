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
		//Get IDG
		bool GetIGD();
		//Open Port
		bool OpenPort(int intPort, int extPort, std::string type);
		//Get Wan IP
		std::string GetWanIP();
		//Get local IP
		std::string GetLocalIP();
		//List Open Ports
		void ListOpenPorts();

	private:
		bool ParseIGD(std::string& lines);
		bool GetRootDesc(std::string xml);
		bool ParseURL(const std::string& url, std::string& host, unsigned short& port, std::string& path);
		bool ParseRootDesc(std::string& rootdesc);
		bool SendSOAPRequest(std::string soaprq, std::string& result);
		std::string AddHTTPtoSoap(std::string soapxml, std::string path, std::string device, std::string action);
		boost::optional<std::string> getXMLValue(const boost::property_tree::ptree& tree, const std::string& path);
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