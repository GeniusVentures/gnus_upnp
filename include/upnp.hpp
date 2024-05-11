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
		UPNP(int intport, int extport)
			: _intport(intport), _extport(extport), _ioc(), _socket(_ioc, boost::asio::ip::udp::v4()),
			_tcpsocket(_ioc, boost::asio::ip::tcp::v4()), _multicast(boost::asio::ip::address_v4({ 239, 255, 255, 250 }), 1900),
			_rootDescXML(""), _requestingRootDesc(false)
		{

		}

		bool GetIGD();
		bool ParseIGD(std::string& lines);
		bool GetRootDesc();
		bool ParseURL(const std::string& url, std::string& host, unsigned short& port, std::string& path);
		bool ParseRootDesc(std::string& rootdesc);
		boost::optional<std::string> getXMLValue(const boost::property_tree::ptree& tree, const std::string& path);

	private:
		int _intport;
		int _extport;
		boost::asio::io_context _ioc;
		boost::asio::ip::udp::socket _socket;
		boost::asio::ip::tcp::socket _tcpsocket;
		const boost::asio::ip::udp::endpoint _multicast;
		std::string _rootDescXML;
		bool _requestingRootDesc;
	};
}

#endif