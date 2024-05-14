#ifndef GNUS_UPNP
#define GNUS_UPNP
#include <boost/asio.hpp>
#include <memory>
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
			_rootDescXML(""), _requestingRootDesc(false), _rootXML()
		{

		}
		//Get IDG
		bool GetIGD();
		//Open Port
		bool OpenPort(int intPort, int extPort, std::string type);
		//Get Wan IP
		bool GetWanIP();

	private:
		bool ParseIGD(std::string& lines);
		bool GetRootDesc();
		bool ParseURL(const std::string& url, std::string& host, unsigned short& port, std::string& path);
		bool ParseRootDesc(std::string& rootdesc);
		boost::optional<std::string> getXMLValue(const boost::property_tree::ptree& tree, const std::string& path);
		//Vars
		boost::asio::io_context _ioc;
		boost::asio::ip::udp::socket _socket;
		boost::asio::ip::tcp::socket _tcpsocket;
		const boost::asio::ip::udp::endpoint _multicast;
		std::string _rootDescXML;
		bool _requestingRootDesc;
		std::string _rootXML;
	};
}

#endif